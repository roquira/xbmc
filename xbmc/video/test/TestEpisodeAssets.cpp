/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "cores/VideoSettings.h"
#include "filesystem/File.h"
#include "filesystem/SpecialProtocol.h"
#include "settings/AdvancedSettings.h"
#include "utils/URIUtils.h"
#include "video/VideoDatabase.h"
#include "video/VideoDatabaseColumns.h"
#include "video/VideoManagerTypes.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
class EpisodeAssetSchemaDatabase : public CDatabase
{
protected:
  void CreateTables() override
  {
    ExecuteQuery(
        "CREATE TABLE path (idPath INTEGER PRIMARY KEY, strPath TEXT, dateAdded TEXT, "
        "idParentPath INTEGER)");
    ExecuteQuery(
        "CREATE TABLE files (idFile INTEGER PRIMARY KEY, idPath INTEGER, strFilename TEXT, "
        "playCount INTEGER, lastPlayed TEXT, dateAdded TEXT)");
    ExecuteQuery("CREATE TABLE tvshow (idShow INTEGER PRIMARY KEY)");
    ExecuteQuery(
        "CREATE TABLE seasons (idSeason INTEGER PRIMARY KEY, idShow INTEGER, season INTEGER)");
    ExecuteQuery(
        "CREATE TABLE episode (idEpisode INTEGER PRIMARY KEY, idFile INTEGER, c12 TEXT, c13 "
        "TEXT, idShow INTEGER, idSeason INTEGER)");
    ExecuteQuery(
        "CREATE TABLE art (art_id INTEGER PRIMARY KEY, media_id INTEGER, media_type TEXT, "
        "type TEXT, url TEXT)");
    ExecuteQuery(
        "CREATE TABLE streamdetails (idFile INTEGER, iStreamType INTEGER, strVideoCodec TEXT, "
        "fVideoAspect FLOAT, iVideoWidth INTEGER, iVideoHeight INTEGER, strAudioCodec TEXT, "
        "iAudioChannels INTEGER, strAudioLanguage TEXT, strSubtitleLanguage TEXT, "
        "iVideoDuration INTEGER, strStereoMode TEXT, strVideoLanguage TEXT, strHdrType TEXT, "
        "strHdrDetail TEXT)");
    ExecuteQuery(
        "CREATE TABLE videoversiontype (id INTEGER PRIMARY KEY, name TEXT, owner INTEGER, "
        "itemType INTEGER)");
    ExecuteQuery(
        "CREATE TABLE videoversion (idFile INTEGER PRIMARY KEY, idMedia INTEGER, media_type "
        "TEXT, itemType INTEGER, idType INTEGER)");
    ExecuteQuery(
        "CREATE TABLE episodeassetcoverage (idFile INTEGER PRIMARY KEY, idShow INTEGER, "
        "idSeason INTEGER, firstSeason INTEGER, firstEpisode INTEGER, lastSeason INTEGER, "
        "lastEpisode INTEGER, disc INTEGER, hasLocator BOOLEAN)");
    ExecuteQuery(
        "CREATE TABLE episodeassetpreference (idEpisode INTEGER PRIMARY KEY, idFile INTEGER)");
    ExecuteQuery(
        "CREATE TABLE seasonreleasepreference (idSeason INTEGER PRIMARY KEY, idType INTEGER)");
    ExecuteQuery(
        "CREATE TABLE tvshowreleasepreference (idShow INTEGER PRIMARY KEY, idType INTEGER)");
    ExecuteQuery(
        "CREATE TABLE tvshowsubtitlepreference (idShow INTEGER PRIMARY KEY, subtitleStream "
        "INTEGER, subtitlesOn BOOLEAN, hasIdentity BOOLEAN, language TEXT, name TEXT, codec TEXT, "
        "flags INTEGER, isExternal BOOLEAN)");
    ExecuteQuery(
        "CREATE TRIGGER delete_tvshow AFTER DELETE ON tvshow FOR EACH ROW BEGIN "
        "DELETE FROM tvshowsubtitlepreference WHERE idShow=old.idShow; END");
  }

  void CreateAnalytics() override {}
  int GetSchemaVersion() const override { return 1; }
  const char* GetBaseDBName() const override { return "EpisodeAssetsTest"; }
};

class EpisodeAssetVideoDatabase : public CVideoDatabase
{
public:
  bool CommitTransaction() override { return CDatabase::CommitTransaction(); }
  int SchemaVersion() const { return GetSchemaVersion(); }
  void UpgradeFrom(int version) { UpdateTables(version); }
};

class EpisodeAssetsTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_settings.type = "sqlite3";
    m_settings.host = CSpecialProtocol::TranslatePath("special://temp/");
    m_databaseName = "EpisodeAssetsTest" + std::to_string(
                                                   std::chrono::steady_clock::now()
                                                       .time_since_epoch()
                                                       .count());
    EpisodeAssetSchemaDatabase schema;
    ASSERT_EQ(CDatabase::ConnectionState::STATE_CONNECTED,
              schema.Connect(m_databaseName, m_settings, true));
    schema.Close();
    ASSERT_EQ(CDatabase::ConnectionState::STATE_CONNECTED,
              m_database.Connect(m_databaseName, m_settings, false));

    ASSERT_TRUE(m_database.ExecuteQuery("INSERT INTO tvshow (idShow) VALUES(1)"));
    ASSERT_TRUE(m_database.ExecuteQuery(
        "INSERT INTO seasons (idSeason, idShow, season) VALUES(10, 1, 1)"));

    m_fileA = m_database.AddFile("C:\\shows\\Show S01E01 [edition=A].mkv");
    ASSERT_GT(m_fileA, 0);
    ASSERT_TRUE(m_database.ExecuteQuery(m_database.PrepareSQL(
        "INSERT INTO episode (idEpisode, idFile, c%02d, c%02d, idShow, idSeason) "
        "VALUES(100, %i, '1', '1', 1, 10)",
        VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_EPISODE_EPISODE, m_fileA)));
  }

  void TearDown() override
  {
    m_database.Close();
    XFILE::CFile::Delete(
        URIUtils::AddFileToFolder(m_settings.host, m_databaseName + ".db"));
  }

  DatabaseSettings m_settings;
  std::string m_databaseName;
  EpisodeAssetVideoDatabase m_database;
  int m_fileA{-1};
};
} // unnamed namespace

TEST_F(EpisodeAssetsTest, ExactVersionsUseDeterministicPreferenceOrder)
{
  EXPECT_EQ(100, m_database.GetEpisodeIdByNumbers(1, 1, 1));
  EXPECT_EQ(-1, m_database.GetEpisodeIdByNumbers(1, 1, 2));

  CFileItem versionA("C:\\shows\\Show S01E01 [edition=A].mkv", false);
  CFileItem versionB("https://media.example/Show%20S01E01%20%5Bedition%3DB%5D.mkv", false);
  ASSERT_TRUE(m_database.AddEpisodeVersion(100, "A", versionA));
  ASSERT_TRUE(m_database.AddEpisodeVersion(100, "B", versionB));

  std::vector<EpisodeAssetInfo> assets;
  m_database.GetEpisodeAssets(100, assets, false);
  ASSERT_EQ(2U, assets.size());

  const auto versionBAsset{
      std::find_if(assets.begin(), assets.end(),
                   [](const auto& asset) { return asset.m_release == "B"; })};
  ASSERT_NE(assets.end(), versionBAsset);
  const int fileB{versionBAsset->m_fileId};

  const int releaseA{assets[0].m_fileId == m_fileA ? assets[0].m_releaseId
                                                    : assets[1].m_releaseId};
  const int releaseB{assets[0].m_fileId == fileB ? assets[0].m_releaseId
                                                 : assets[1].m_releaseId};

  ASSERT_TRUE(m_database.SetTvShowPreferredRelease(1, releaseB));
  EXPECT_EQ(fileB, m_database.ResolveEpisodeAssetId(100));

  ASSERT_TRUE(m_database.SetSeasonPreferredRelease(10, releaseA));
  EXPECT_EQ(m_fileA, m_database.ResolveEpisodeAssetId(100));

  ASSERT_TRUE(m_database.SetEpisodeDefaultAsset(100, fileB));
  EXPECT_EQ(fileB, m_database.ResolveEpisodeAssetId(100));

  EXPECT_TRUE(m_database.SetEpisodeDefaultAsset(100, -1));
  EXPECT_TRUE(m_database.SetSeasonPreferredRelease(10, -1));
  EXPECT_TRUE(m_database.SetTvShowPreferredRelease(1, -1));
}

TEST_F(EpisodeAssetsTest, ResolvedDefaultDoesNotRequireLogicalFileToBeVersion)
{
  CFileItem versionB("C:\\shows\\Show S01E01 [edition=B].mkv", false);
  CFileItem versionC("C:\\shows\\Show S01E01 [edition=C].mkv", false);
  ASSERT_TRUE(m_database.AddEpisodeVersion(100, "B", versionB));
  ASSERT_TRUE(m_database.AddEpisodeVersion(100, "C", versionC));

  // The logical watched-state file was never added as a version. Presentation must still mark
  // exactly one of the real exact assets using the same fallback that playback resolves.
  const int resolvedFile{m_database.ResolveEpisodeAssetId(100)};
  ASSERT_GT(resolvedFile, 0);
  EXPECT_NE(m_fileA, resolvedFile);

  std::vector<EpisodeAssetInfo> assets;
  m_database.GetEpisodeAssets(100, assets, false);
  ASSERT_EQ(2U, assets.size());
  EXPECT_NE(assets.end(), std::ranges::find(assets, resolvedFile, &EpisodeAssetInfo::m_fileId));
}

TEST_F(EpisodeAssetsTest, RangeAssetCountsMatchEpisodeBrowserVisibility)
{
  CFileItem firstDisc(
      "C:\\shows\\Show S01E01-E07 [edition=Blu-ray] [disc=01]\\BDMV\\index.bdmv", false);
  ASSERT_TRUE(m_database.AddEpisodeRangeAsset(1, 10, 1, 1, 1, 7, 1, "Blu-ray", firstDisc,
                                              false));

  std::vector<EpisodeAssetInfo> assets;
  m_database.GetEpisodeAssets(100, assets, false);
  EXPECT_TRUE(assets.empty());
  m_database.GetEpisodeAssets(100, assets, true);
  ASSERT_EQ(1U, assets.size());
  EXPECT_FALSE(assets.front().IsExact());

  CFileItem secondDisc(
      "C:\\shows\\Show S01E01-E04 [edition=Alternate Blu-ray] [disc=01]\\BDMV\\index.bdmv",
      false);
  ASSERT_TRUE(m_database.AddEpisodeRangeAsset(1, 10, 1, 1, 1, 4, 1, "Alternate Blu-ray",
                                              secondDisc, false));
  m_database.GetEpisodeAssets(100, assets, true);
  EXPECT_EQ(2U, assets.size());

  CFileItem exact("C:\\shows\\Show S01E01 [edition=WEB-DL].mkv", false);
  ASSERT_TRUE(m_database.AddEpisodeVersion(100, "WEB-DL", exact));
  m_database.GetEpisodeAssets(100, assets, false);
  ASSERT_EQ(1U, assets.size());
  EXPECT_TRUE(assets.front().IsExact());
  m_database.GetEpisodeAssets(100, assets, true);
  EXPECT_EQ(3U, assets.size());
}

TEST_F(EpisodeAssetsTest, RangeWithoutLocatorIsStoredButNeverResolvedAsEpisode)
{
  EXPECT_FALSE(m_database.HasSeasonRangeAssets(10));

  CFileItem version("C:\\shows\\Show S01E01.mkv", false);
  ASSERT_TRUE(m_database.AddEpisodeVersion(100, "WEB-DL", version));

  const std::string discPath{
      "https://media.example/Show%20S01E01-E07%20%5Bedition%3DBlu-ray%5D/BDMV/index.bdmv"};
  CFileItem disc(discPath, false);
  ASSERT_TRUE(m_database.AddEpisodeRangeAsset(1, 10, 1, 1, 1, 7, 1, "Blu-ray", disc,
                                              false));
  EXPECT_TRUE(m_database.HasSeasonRangeAssets(10));

  std::vector<EpisodeAssetInfo> assets;
  m_database.GetEpisodeAssets(100, assets, true);
  ASSERT_EQ(2U, assets.size());
  const auto range{std::find_if(assets.begin(), assets.end(),
                                [](const auto& asset) { return !asset.IsExact(); })};
  ASSERT_NE(assets.end(), range);
  EXPECT_FALSE(range->m_hasLocator);
  EXPECT_EQ(discPath, range->m_path);

  EpisodeAssetInfo pathAsset;
  ASSERT_TRUE(m_database.GetEpisodeRangeAssetByPath(discPath, pathAsset));
  EXPECT_EQ(range->m_fileId, pathAsset.m_fileId);
  EXPECT_EQ("Blu-ray", pathAsset.m_release);
  EXPECT_EQ(1, pathAsset.m_firstEpisode);
  EXPECT_EQ(7, pathAsset.m_lastEpisode);
  EXPECT_FALSE(m_database.GetEpisodeRangeAssetByPath("C:\\shows\\not-a-disc.mkv", pathAsset));

  std::vector<EpisodeAssetInfo> exactOnlyAssets;
  m_database.GetEpisodeAssets(100, exactOnlyAssets, false);
  ASSERT_EQ(1U, exactOnlyAssets.size());
  EXPECT_TRUE(exactOnlyAssets.front().IsExact());

  const auto exact{std::find_if(assets.begin(), assets.end(),
                                [](const auto& asset) { return asset.IsExact(); })};
  ASSERT_NE(assets.end(), exact);
  EXPECT_EQ(exact->m_fileId, m_database.ResolveEpisodeAssetId(100));
  EXPECT_NE(range->m_fileId, m_database.ResolveEpisodeAssetId(100));

  const int fileE06{m_database.AddFile("C:\\shows\\Show S01E06.mkv")};
  const int fileE07{m_database.AddFile("C:\\shows\\Show S01E07.mkv")};
  const int fileE08{m_database.AddFile("C:\\shows\\Show S01E08.mkv")};
  ASSERT_GT(fileE06, 0);
  ASSERT_GT(fileE07, 0);
  ASSERT_GT(fileE08, 0);
  ASSERT_TRUE(m_database.ExecuteQuery(m_database.PrepareSQL(
      "INSERT INTO episode (idEpisode, idFile, c%02d, c%02d, idShow, idSeason) VALUES "
      "(106, %i, '1', '6', 1, 10), (107, %i, '1', '7', 1, 10), "
      "(108, %i, '1', '8', 1, 10)",
      VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_EPISODE_EPISODE, fileE06, fileE07,
      fileE08)));

  CFileItem secondDisc(
      "C:\\shows\\Show S01E08-E11 [edition=Blu-ray] [disc=02]\\BDMV\\index.bdmv", false);
  ASSERT_TRUE(m_database.AddEpisodeRangeAsset(1, 10, 1, 8, 1, 11, 2, "Blu-ray", secondDisc,
                                              false));

  const auto rangeFileForEpisode = [this](int episodeId)
  {
    std::vector<EpisodeAssetInfo> episodeAssets;
    m_database.GetEpisodeAssets(episodeId, episodeAssets, true);
    const auto rangeAsset{std::ranges::find_if(
        episodeAssets, [](const auto& asset) { return !asset.IsExact(); })};
    return rangeAsset == episodeAssets.end() ? -1 : rangeAsset->m_fileId;
  };
  EXPECT_EQ(range->m_fileId, rangeFileForEpisode(106));
  EXPECT_EQ(range->m_fileId, rangeFileForEpisode(107));
  const int secondDiscFile{rangeFileForEpisode(108)};
  EXPECT_GT(secondDiscFile, 0);
  EXPECT_NE(range->m_fileId, secondDiscFile);

  ASSERT_TRUE(m_database.ExecuteQuery(
      "INSERT INTO seasons (idSeason, idShow, season) VALUES(20, 1, 2)"));
  CFileItem thirdDisc("C:\\shows\\Show S02E01-E04 [edition=Blu-ray] [disc=03]\\BDMV\\index.bdmv",
                      false);
  ASSERT_TRUE(m_database.AddEpisodeRangeAsset(1, 20, 2, 1, 2, 4, 3, "Blu-ray", thirdDisc,
                                              false));

  std::vector<EpisodeAssetInfo> showRanges;
  m_database.GetTvShowRangeAssets(1, range->m_releaseId, showRanges);
  ASSERT_EQ(3U, showRanges.size());
  EXPECT_EQ(std::pair(1, 1),
            std::pair(showRanges[0].m_firstSeason, showRanges[0].m_firstEpisode));
  EXPECT_EQ(std::pair(1, 8),
            std::pair(showRanges[1].m_firstSeason, showRanges[1].m_firstEpisode));
  EXPECT_EQ(std::pair(2, 1),
            std::pair(showRanges[2].m_firstSeason, showRanges[2].m_firstEpisode));
}

TEST_F(EpisodeAssetsTest, TvShowSubtitlePreferenceRoundTripAndOverwrite)
{
  EXPECT_EQ(149, m_database.SchemaVersion());

  CTVShowSubtitlePreference preference;
  preference.m_stream = 1;
  preference.m_enabled = true;
  preference.m_hasIdentity = true;
  preference.m_language = "jpn";
  preference.m_name = "Signs & Songs";
  preference.m_codec = "ass";
  preference.m_flags = StreamFlags::FLAG_DEFAULT;
  preference.m_isExternal = false;
  ASSERT_TRUE(m_database.SetTvShowSubtitlePreference(1, preference));

  CTVShowSubtitlePreference stored;
  ASSERT_TRUE(m_database.GetTvShowSubtitlePreference(1, stored));
  EXPECT_EQ(1, stored.m_stream);
  EXPECT_TRUE(stored.m_enabled);
  EXPECT_TRUE(stored.m_hasIdentity);
  EXPECT_EQ("jpn", stored.m_language);
  EXPECT_EQ("Signs & Songs", stored.m_name);
  EXPECT_EQ("ass", stored.m_codec);
  EXPECT_EQ(StreamFlags::FLAG_DEFAULT, stored.m_flags);
  EXPECT_FALSE(stored.m_isExternal);

  preference.m_stream = 0;
  preference.m_enabled = false;
  preference.m_name = "Full subtitles";
  preference.m_flags = StreamFlags::FLAG_HEARING_IMPAIRED;
  preference.m_isExternal = true;
  ASSERT_TRUE(m_database.SetTvShowSubtitlePreference(1, preference));
  ASSERT_TRUE(m_database.GetTvShowSubtitlePreference(1, stored));
  EXPECT_EQ(0, stored.m_stream);
  EXPECT_FALSE(stored.m_enabled);
  EXPECT_EQ("Full subtitles", stored.m_name);
  EXPECT_EQ(StreamFlags::FLAG_HEARING_IMPAIRED, stored.m_flags);
  EXPECT_TRUE(stored.m_isExternal);

  EXPECT_TRUE(m_database.EraseTvShowSubtitlePreference(1));
  EXPECT_FALSE(m_database.GetTvShowSubtitlePreference(1, stored));
  EXPECT_FALSE(m_database.SetTvShowSubtitlePreference(999, preference));
}

TEST_F(EpisodeAssetsTest, DeletingTvShowErasesSubtitlePreference)
{
  CTVShowSubtitlePreference preference;
  preference.m_stream = 0;
  ASSERT_TRUE(m_database.SetTvShowSubtitlePreference(1, preference));

  ASSERT_TRUE(m_database.ExecuteQuery("DELETE FROM tvshow WHERE idShow=1"));

  CTVShowSubtitlePreference stored;
  EXPECT_FALSE(m_database.GetTvShowSubtitlePreference(1, stored));
}

TEST_F(EpisodeAssetsTest, MigratesTvShowSubtitlePreferenceFromVersion148WithAnalyticsAbsent)
{
  ASSERT_TRUE(m_database.ExecuteQuery("DROP TRIGGER delete_tvshow"));
  ASSERT_TRUE(m_database.ExecuteQuery("DROP TABLE tvshowsubtitlepreference"));

  m_database.UpgradeFrom(148);

  CTVShowSubtitlePreference preference;
  preference.m_stream = 1;
  ASSERT_TRUE(m_database.SetTvShowSubtitlePreference(1, preference));
  CTVShowSubtitlePreference stored;
  ASSERT_TRUE(m_database.GetTvShowSubtitlePreference(1, stored));
  EXPECT_EQ(1, stored.m_stream);
}
