/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ServiceBroker.h"
#include "cores/IPlayerCallback.h"
#include "cores/VideoPlayer/VideoPlayer.h"
#include "jobs/JobManager.h"

#include <gtest/gtest.h>

using namespace std::chrono_literals;

class CFileItem;

class CTestPlayerCallback : public IPlayerCallback
{
public:
  void OnPlayBackEnded() override {}
  void OnPlayBackStarted(const CFileItem& file) override {}
  void OnPlayBackStopped() override {}
  void OnPlayBackError() override {}
  void OnQueueNextItem() override {}
};

class CTestVideoPlayer : public CVideoPlayer
{
public:
  explicit CTestVideoPlayer(IPlayerCallback& c) : CVideoPlayer(c) {}
  virtual ~CTestVideoPlayer() {}

  int InvokeGetPreviousBookmark(std::chrono::milliseconds ts) { return GetPreviousBookmark(ts); }
  int InvokeGetNextBookmark(std::chrono::milliseconds ts) { return GetNextBookmark(ts); }
  std::optional<std::chrono::milliseconds> InvokeGetBookmarkPos(int idx)
  {
    return GetBookmarkPos(idx);
  }
};

class TestVideoPlayer : public testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    CServiceBroker::RegisterJobManager(std::make_shared<CJobManager>());
  }
  static void TearDownTestSuite() { CServiceBroker::UnregisterJobManager(); }
};

TEST_F(TestVideoPlayer, GetPreviousBookmark)
{
  CTestPlayerCallback playercallback;
  CTestVideoPlayer player(playercallback);

  std::vector<std::chrono::milliseconds> bookmarks{100s, 200s};
  player.SetBookmarks(bookmarks);

  std::chrono::milliseconds ts{0s};
  int idx = player.InvokeGetPreviousBookmark(ts);
  EXPECT_EQ(-1, idx);
  ts = 100s;
  idx = player.InvokeGetPreviousBookmark(ts);
  EXPECT_EQ(-1, idx);
  // 5-second grade delay
  ts = 105s;
  idx = player.InvokeGetPreviousBookmark(ts);
  EXPECT_EQ(-1, idx);
  ts = 106s;
  idx = player.InvokeGetPreviousBookmark(ts);
  EXPECT_EQ(0, idx);
  ts = 200s;
  idx = player.InvokeGetPreviousBookmark(ts);
  EXPECT_EQ(0, idx);
  // 5-second grade delay
  ts = 205s;
  idx = player.InvokeGetPreviousBookmark(ts);
  EXPECT_EQ(0, idx);
  ts = 206s;
  idx = player.InvokeGetPreviousBookmark(ts);
  EXPECT_EQ(1, idx);
}

TEST_F(TestVideoPlayer, GetNextBookmark)
{
  CTestPlayerCallback playercallback;
  CTestVideoPlayer player(playercallback);

  std::vector<std::chrono::milliseconds> bookmarks{100s, 200s};
  player.SetBookmarks(bookmarks);

  std::chrono::milliseconds ts{0s};
  int idx = player.InvokeGetNextBookmark(ts);
  EXPECT_EQ(0, idx);
  ts = 100s;
  idx = player.InvokeGetNextBookmark(ts);
  EXPECT_EQ(1, idx);
  ts = 101s;
  idx = player.InvokeGetNextBookmark(ts);
  EXPECT_EQ(1, idx);
  ts = 200s;
  idx = player.InvokeGetNextBookmark(ts);
  EXPECT_EQ(-1, idx);
  ts = 201s;
  idx = player.InvokeGetNextBookmark(ts);
  EXPECT_EQ(-1, idx);
}

TEST_F(TestVideoPlayer, TvShowSubtitlePreferenceUsesIdentityBeforeOrdinal)
{
  CTVShowSubtitlePreference preference;
  preference.m_stream = 0;
  preference.m_hasIdentity = true;
  preference.m_language = "jpn";
  preference.m_name = "Signs & Songs";
  preference.m_codec = "ass";
  preference.m_flags = StreamFlags::FLAG_DEFAULT;

  SelectionStream ordinal;
  ordinal.type = StreamType::SUBTITLE;
  ordinal.type_index = 0;
  ordinal.language = "eng";
  ordinal.name = "Full subtitles";
  ordinal.codec = "ass";

  SelectionStream identity{ordinal};
  identity.type_index = 1;
  identity.language = "ja";
  identity.name = " signs & songs ";
  identity.codec = " ASS ";
  identity.flags = StreamFlags::FLAG_DEFAULT;

  EXPECT_FALSE(TVShowSubtitlePreferenceMatches(preference, ordinal));
  EXPECT_TRUE(TVShowSubtitlePreferenceMatches(preference, identity));
  EXPECT_LT(TVShowSubtitlePreferencePriority(preference, ordinal),
            TVShowSubtitlePreferencePriority(preference, identity));
}

TEST_F(TestVideoPlayer, TvShowSubtitlePreferenceBreaksIdentityTieWithOrdinal)
{
  CTVShowSubtitlePreference preference;
  preference.m_stream = 1;
  preference.m_hasIdentity = true;
  preference.m_language = "eng";
  preference.m_name = "Full subtitles";
  preference.m_codec = "ass";

  SelectionStream first;
  first.type = StreamType::SUBTITLE;
  first.type_index = 0;
  first.language = "eng";
  first.name = "Full subtitles";
  first.codec = "ass";

  SelectionStream second{first};
  second.type_index = 1;

  EXPECT_TRUE(TVShowSubtitlePreferenceMatches(preference, first));
  EXPECT_TRUE(TVShowSubtitlePreferenceMatches(preference, second));
  EXPECT_LT(TVShowSubtitlePreferencePriority(preference, first),
            TVShowSubtitlePreferencePriority(preference, second));
}

TEST_F(TestVideoPlayer, TvShowSubtitlePreferenceFallsBackToOrdinal)
{
  CTVShowSubtitlePreference preference;
  preference.m_stream = 2;
  preference.m_hasIdentity = true;
  preference.m_language = "jpn";
  preference.m_name = "Signs & Songs";
  preference.m_codec = "ass";

  SelectionStream fallback;
  fallback.type = StreamType::SUBTITLE;
  fallback.type_index = 2;
  fallback.language = "eng";
  fallback.name = "Full subtitles";
  fallback.codec = "srt";

  SelectionStream normal{fallback};
  normal.type_index = 0;

  EXPECT_EQ(1, TVShowSubtitlePreferencePriority(preference, fallback));
  EXPECT_EQ(0, TVShowSubtitlePreferencePriority(preference, normal));

  preference.m_stream = 9;
  EXPECT_EQ(0, TVShowSubtitlePreferencePriority(preference, fallback));
  EXPECT_EQ(0, TVShowSubtitlePreferencePriority(preference, normal));
}

TEST_F(TestVideoPlayer, TvShowSubtitlePreferenceIdentityIncludesFlagsAndExternalStatus)
{
  CTVShowSubtitlePreference preference;
  preference.m_hasIdentity = true;
  preference.m_language = "eng";
  preference.m_name = "Full subtitles";
  preference.m_codec = "ass";
  preference.m_flags = StreamFlags::FLAG_HEARING_IMPAIRED;
  preference.m_isExternal = true;

  SelectionStream stream;
  stream.type = StreamType::SUBTITLE;
  stream.language = "eng";
  stream.name = "Full subtitles";
  stream.codec = "ass";
  stream.flags = StreamFlags::FLAG_HEARING_IMPAIRED;
  stream.source = STREAM_SOURCE_DEMUX_SUB;
  EXPECT_TRUE(TVShowSubtitlePreferenceMatches(preference, stream));

  stream.flags = StreamFlags::FLAG_NONE;
  EXPECT_FALSE(TVShowSubtitlePreferenceMatches(preference, stream));
  stream.flags = StreamFlags::FLAG_HEARING_IMPAIRED;
  stream.source = STREAM_SOURCE_DEMUX;
  EXPECT_FALSE(TVShowSubtitlePreferenceMatches(preference, stream));
}

TEST_F(TestVideoPlayer, TvShowSubtitlePreferenceIsExcludedFromVideoSettingsEquality)
{
  CVideoSettings left;
  CVideoSettings right;
  CTVShowSubtitlePreference preference;
  preference.m_stream = 1;
  right.m_tvShowSubtitlePreference = preference;
  right.m_tvShowSubtitlePreferenceDirty = true;

  EXPECT_FALSE(left != right);
  EXPECT_FALSE(right != left);
}

TEST_F(TestVideoPlayer, GetBookmarkPos)
{
  CTestPlayerCallback playercallback;
  CTestVideoPlayer player(playercallback);

  std::vector<std::chrono::milliseconds> bookmarks{100s, 200s};
  player.SetBookmarks(bookmarks);

  std::optional<std::chrono::milliseconds> pos = player.InvokeGetBookmarkPos(-1);

  EXPECT_FALSE(pos.has_value());

  pos = player.InvokeGetBookmarkPos(0);
  EXPECT_TRUE(pos.has_value());
  if (pos.has_value())
  {
    // braces to quiet clang warning
    EXPECT_EQ(100s, pos.value());
  }

  pos = player.InvokeGetBookmarkPos(1);
  EXPECT_TRUE(pos.has_value());
  if (pos.has_value())
  {
    // braces to quiet clang warning
    EXPECT_EQ(200s, pos.value());
  }

  pos = player.InvokeGetBookmarkPos(2);
  EXPECT_FALSE(pos.has_value());
}
