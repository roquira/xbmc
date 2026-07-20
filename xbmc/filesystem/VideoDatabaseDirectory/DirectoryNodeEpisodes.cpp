/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DirectoryNodeEpisodes.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "QueryParams.h"
#include "ServiceBroker.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"
#include "video/VideoDatabase.h"
#include "video/VideoDbUrl.h"
#include "video/VideoManagerTypes.h"

#include <map>

using namespace XFILE::VIDEODATABASEDIRECTORY;

CDirectoryNodeEpisodes::CDirectoryNodeEpisodes(const std::string& strName, CDirectoryNode* pParent)
  : CDirectoryNode(NodeType::EPISODES, strName, pParent)
{

}

bool CDirectoryNodeEpisodes::GetContent(CFileItemList& items) const
{
  CVideoDatabase videodatabase;
  if (!videodatabase.Open())
    return false;

  CQueryParams params;
  CollectQueryParams(params);

  int season = (int)params.GetSeason();
  if (season == -2)
    season = -1;

  int details = items.HasProperty("set_videodb_details")
                    ? items.GetProperty("set_videodb_details").asInteger32()
                    : VideoDbDetailsNone;

  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(BuildPath()))
    return false;

  const int idSeason{videodatabase.GetSeasonId(params.GetTvShowId(), season)};
  CVariant episodeAssetsOption;
  if (videoUrl.GetOption("episodeassets", episodeAssetsOption) &&
      episodeAssetsOption.asBoolean(false))
  {
    std::vector<EpisodeAssetInfo> assets;
    CVariant releaseOption;
    const int releaseId{videoUrl.GetOption("releaseid", releaseOption)
                            ? releaseOption.asInteger32()
                            : -1};
    videodatabase.GetSeasonRangeAssets(idSeason, releaseId, assets);

    if (releaseId < 0)
    {
      std::map<int, std::string> releases;
      for (const auto& asset : assets)
        releases.try_emplace(asset.m_releaseId, asset.m_release);

      for (const auto& [id, release] : releases)
      {
        auto item{std::make_shared<CFileItem>(release)};
        CVideoDbUrl itemUrl{videoUrl};
        itemUrl.AddOption("releaseid", id);
        item->SetPath(itemUrl.ToString());
        item->SetFolder(true);
        item->SetCanQueue(false);
        item->SetProperty("episode_release_group", true);
        item->SetProperty("episode_release_id", id);
        item->SetProperty("episode_release_show_id", static_cast<int>(params.GetTvShowId()));
        item->SetProperty("episode_release_season_id", idSeason);
        items.Add(std::move(item));
      }
    }
    else
    {
      for (const auto& asset : assets)
      {
        if (asset.m_releaseId != releaseId)
          continue;

        const std::string coverage{
            asset.m_firstSeason == asset.m_lastSeason
                ? StringUtils::Format("E{:02}-E{:02}", asset.m_firstEpisode,
                                      asset.m_lastEpisode)
                : StringUtils::Format("S{:02}E{:02}-S{:02}E{:02}", asset.m_firstSeason,
                                      asset.m_firstEpisode, asset.m_lastSeason,
                                      asset.m_lastEpisode)};
        const std::string disc{asset.m_disc > 0
                                   ? StringUtils::Format(
                                         CServiceBroker::GetResourcesComponent()
                                             .GetLocalizeStrings()
                                             .Get(40814),
                                         asset.m_disc)
                                   : CServiceBroker::GetResourcesComponent()
                                         .GetLocalizeStrings()
                                         .Get(40815)};
        auto item{std::make_shared<CFileItem>(asset.m_path, false)};
        item->SetDynPath(asset.m_path);
        item->SetLabel(StringUtils::Format("{} - {}", disc, coverage));
        item->SetLabelPreformatted(true);
        item->SetProperty("episode_range_asset", true);
        items.Add(std::move(item));
      }
    }
    items.SetContent("files");
    videodatabase.Close();
    return true;
  }

  bool bSuccess = videodatabase.GetEpisodesNav(
      BuildPath(), items, params.GetGenreId(), params.GetYear(), params.GetActorId(),
      params.GetDirectorId(), params.GetTvShowId(), season, SortDescription(), details);

  if (bSuccess && idSeason > 0 && videodatabase.HasSeasonRangeAssets(idSeason))
  {
    auto releases{std::make_shared<CFileItem>(
        CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(40813))};
    videoUrl.AddOption("episodeassets", true);
    videoUrl.RemoveOption("releaseid");
    releases->SetPath(videoUrl.ToString());
    releases->SetFolder(true);
    releases->SetCanQueue(false);
    releases->SetLabelPreformatted(true);
    items.Add(std::move(releases));
  }

  videodatabase.Close();

  return bSuccess;
}

NodeType CDirectoryNodeEpisodes::GetChildType() const
{
  return NodeType::EPISODES;
}
