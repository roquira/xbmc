/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "filesystem/VideoDatabaseDirectory/DirectoryNode.h"
#include "filesystem/VideoDatabaseDirectory/QueryParams.h"
#include "video/VideoDatabase.h"

#include <gtest/gtest.h>

#include <memory>

using namespace XFILE::VIDEODATABASEDIRECTORY;

TEST(TestNodeEpisodeAssets, ParsesEpisodeVersionDirectory)
{
  std::unique_ptr<CDirectoryNode> node{
      CDirectoryNode::ParseURL("videodb://tvshows/titles/42/1/99/")};

  ASSERT_TRUE(node);
  EXPECT_EQ(NodeType::EPISODES, node->GetType());
  EXPECT_EQ(NodeType::EPISODE_ASSETS, node->GetChildType());
  ASSERT_TRUE(node->GetParent());
  EXPECT_EQ(NodeType::SEASONS, node->GetParent()->GetType());

  CQueryParams params;
  node->CollectQueryParams(params);
  EXPECT_EQ(VideoDbContentType::TVSHOWS,
            static_cast<VideoDbContentType>(params.GetContentType()));
  EXPECT_EQ(42, params.GetTvShowId());
  EXPECT_EQ(1, params.GetSeason());
  EXPECT_EQ(99, params.GetEpisodeId());
}
