/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DirectoryNodeEpisodeAssets.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "QueryParams.h"
#include "ServiceBroker.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "video/VideoDatabase.h"

using namespace XFILE::VIDEODATABASEDIRECTORY;

namespace
{
std::string GetRangeLabel(const CFileItem& item)
{
  const int firstSeason{item.GetProperty("episode_range_first_season").asInteger32()};
  const int firstEpisode{item.GetProperty("episode_range_first_episode").asInteger32()};
  const int lastSeason{item.GetProperty("episode_range_last_season").asInteger32()};
  const int lastEpisode{item.GetProperty("episode_range_last_episode").asInteger32()};
  const std::string coverage{
      firstSeason == lastSeason
          ? StringUtils::Format("E{:02}-E{:02}", firstEpisode, lastEpisode)
          : StringUtils::Format("S{:02}E{:02}-S{:02}E{:02}", firstSeason, firstEpisode,
                                lastSeason, lastEpisode)};
  const auto& localize{CServiceBroker::GetResourcesComponent().GetLocalizeStrings()};
  const int disc{item.GetProperty("episode_range_disc").asInteger32()};
  const std::string discLabel{
      disc > 0 ? StringUtils::Format(localize.Get(40814), disc) : localize.Get(40815)};
  return StringUtils::Format("{} - {} - {}", item.GetLabel(), discLabel, coverage);
}
} // unnamed namespace

CDirectoryNodeEpisodeAssets::CDirectoryNodeEpisodeAssets(const std::string& strName,
                                                         CDirectoryNode* pParent)
  : CDirectoryNode(NodeType::EPISODE_ASSETS, strName, pParent)
{
}

bool CDirectoryNodeEpisodeAssets::GetContent(CFileItemList& items) const
{
  CQueryParams params;
  CollectQueryParams(params);
  if (params.GetEpisodeId() <= 0)
    return false;

  CVideoDatabase database;
  if (!database.Open())
    return false;

  const bool includeRangeAssets{
      CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
          CSettings::SETTING_VIDEOLIBRARY_SHOWUNLOCATEDEPISODEDISCS)};
  database.GetEpisodeVersions(params.GetEpisodeId(), items, includeRangeAssets);
  if (!items.IsEmpty() && items.Get(0)->HasVideoInfoTag())
    items.SetProperty("customtitle", items.Get(0)->GetVideoInfoTag()->GetTitle());

  for (const auto& item : items)
  {
    if (item->GetProperty("episode_range_asset").asBoolean(false))
    {
      item->SetLabel(GetRangeLabel(*item));
      item->SetLabelPreformatted(true);
    }
    item->SetProperty("episode_asset_selected", true);
    item->SetProperty("original_listitem_url", BuildPath());
    if (item->HasVideoInfoTag() && item->GetVideoInfoTag()->IsDefaultVideoVersion())
      item->Select(true);
  }

  database.Close();
  return !items.IsEmpty();
}

NodeType CDirectoryNodeEpisodeAssets::GetChildType() const
{
  return NodeType::EPISODES;
}

std::string CDirectoryNodeEpisodeAssets::GetLocalizedName() const
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(40210);
}
