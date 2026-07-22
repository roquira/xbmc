/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "cores/VideoPlayer/DVDFileInfo.h"

#include <gtest/gtest.h>

TEST(TestDVDFileInfo, CanExtractFromLanHttpFile)
{
  EXPECT_TRUE(CDVDFileInfo::CanExtract(CFileItem("http://media-server/movie.mkv", false)));
  EXPECT_TRUE(CDVDFileInfo::CanExtract(CFileItem("https://media-server/movie.mkv", false)));
  EXPECT_FALSE(CDVDFileInfo::CanExtract(CFileItem("https://media.example/movie.mkv", false)));
}
