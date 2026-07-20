/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "DirectoryNode.h"

namespace XFILE
{
namespace VIDEODATABASEDIRECTORY
{
class CDirectoryNodeEpisodeAssets : public CDirectoryNode
{
public:
  CDirectoryNodeEpisodeAssets(const std::string& strEntryName, CDirectoryNode* pParent);

  std::string GetLocalizedName() const override;

protected:
  bool GetContent(CFileItemList& items) const override;
  NodeType GetChildType() const override;
};
} // namespace VIDEODATABASEDIRECTORY
} // namespace XFILE
