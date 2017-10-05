/*
 *      Copyright (C) 2017 Wolfgang Haupt
 *
 *      Copyright (C) 2013-2015 Anton Fedchin
 *      http://github.com/afedchin/xbmc-addon-iptvsimple/
 *
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <sstream>
#include <string>
#include <fstream>
#include <map>
#include <stdexcept>
#include "zlib.h"
#include "rapidxml/rapidxml.hpp"
#include "PVRIptvData.h"
#include "p8-platform/util/StringUtils.h"
#include "CurlHelper.h"
#include "rapidjson/document.h"

#define TVG_INFO_ID_MARKER      "tvg-id="
#define TVG_INFO_NAME_MARKER    "tvg-name="
#define TVG_INFO_LOGO_MARKER    "tvg-logo="
#define TVG_INFO_SHIFT_MARKER   "tvg-shift="
#define TVG_INFO_CHNO_MARKER    "tvg-chno="
#define GROUP_NAME_MARKER       "group-title="
#define RADIO_MARKER            "radio="
#define CHANNEL_LOGO_EXTENSION  ".png"
#define SECONDS_IN_DAY          86400
#define GENRES_MAP_FILENAME     "genres.xml"

using namespace ADDON;
using namespace rapidxml;

template<class Ch>
inline bool GetNodeValue(const xml_node<Ch> * pRootNode, const char* strTag, std::string& strStringValue)
{
  xml_node<Ch> *pChildNode = pRootNode->first_node(strTag);
  if (pChildNode == NULL)
  {
    return false;
  }
  strStringValue = pChildNode->value();
  return true;
}

template<class Ch>
inline bool GetAttributeValue(const xml_node<Ch> * pNode, const char* strAttributeName, std::string& strStringValue)
{
  xml_attribute<Ch> *pAttribute = pNode->first_attribute(strAttributeName);
  if (pAttribute == NULL)
  {
    return false;
  }
  strStringValue = pAttribute->value();
  return true;
}

PVRIptvData::PVRIptvData(void)
{
  m_strLogoPath   = g_strLogoPath;
  m_iEPGTimeShift = g_iEPGTimeShift;
  m_bTSOverride   = g_bTSOverride;
  m_iLastStart    = 0;
  m_iLastEnd      = 0;

  m_strRestUrl    = g_strRestUrl;
  m_strEpgRestUrl = g_strEpgRestUrl;

  m_channels.clear();
  m_groups.clear();
  m_epg.clear();

  LoadPlayList();

  for (auto const& channel : m_channels)
  {
    PVRIptvEpgChannel epgChannel;
    epgChannel.strId = std::to_string(channel.iChannelNumber);
    epgChannel.strName = channel.strChannelName;

    m_epg.push_back(epgChannel);
  }
}

void *PVRIptvData::Process(void)
{
  return NULL;
}

PVRIptvData::~PVRIptvData(void)
{
  m_channels.clear();
  m_groups.clear();
  m_epg.clear();
}

void PVRIptvData::ReloadPlayList()
{
  m_channels.clear();
  m_groups.clear();

  if (LoadPlayList())
  {
    PVR->TriggerChannelGroupsUpdate();
    PVR->TriggerChannelUpdate();
  }
}

bool PVRIptvData::LoadPlayList(void)
{
  std::string strChannelsFromUrl("");
  strChannelsFromUrl = grabChannels(m_strRestUrl);

  rapidjson::Document doc;
  doc.Parse(strChannelsFromUrl.c_str());

  if (!doc.IsObject())
  {
    XBMC->Log(LOG_ERROR, "Cannot load channels - Invalid json format");
    return false;
  }
  
  if (!doc.HasMember("groups"))
  {
    XBMC->Log(LOG_ERROR, "Cannot load channels - json response has no groups");
    return false;
  }

  if (!doc["groups"].IsArray())
  {
    XBMC->Log(LOG_ERROR, "Cannot load channels - groups element is not an array");
    return false;
  }

  for (auto& g : doc["groups"].GetArray())
  {
    PVRIptvChannelGroup group;
    if (g["id"].IsInt() && g["name"].IsString())
    {
      group.iGroupId = g["id"].GetInt();
      group.strGroupName = g["name"].GetString();
      group.bRadio = false;      //we do not support radio right now
      m_groups.push_back(group);
    }
    else
    {
      XBMC->Log(LOG_ERROR, "Group format is wrong - skip group, this may be ok");
    }
  }

  int iChannelIndex = 0;
  if (!doc.HasMember("channels"))
  {
    XBMC->Log(LOG_ERROR, "Cannot load channels - json response has no channels");
    return false;
  }

  if (!doc["channels"].IsArray())
  {
    XBMC->Log(LOG_ERROR, "Cannot load channels - channels member is not an array");
    return false;
  }

  for (auto& c : doc["channels"].GetArray())
  {
    PVRIptvChannel channel;

    //to remove the need of holding a channel mapping inside the app, we don't use
    //the channel_id field anymore but reuse the channel_number
    channel.iUniqueId = c["channel_number"].IsInt() ? c["channel_number"].GetInt() : -1;

    channel.strChannelName    = c["name"].IsString() ? c["name"].GetString() : "";
    channel.iChannelNumber    = c["channel_number"].IsInt() ? c["channel_number"].GetInt() : 0;
    channel.strStreamURL      = c["url"].IsString() ? c["url"].GetString() : "";
    channel.strLogoPath       = c["logo"].IsString() ? c["logo"].GetString() : "";
    channel.strTvgName        = c["name"].IsString() ? c["name"].GetString() : "";
    channel.strTvgLogo        = c["logo"].IsString() ? c["logo"].GetString() : "";
    channel.strTvgId          = "";
    channel.iTvgShift         = 0;
    channel.bRadio            = false;
    channel.iEncryptionSystem = 0;

    if (c["group"].IsInt())
      m_groups.at(c["group"].GetInt() - 1).members.push_back(iChannelIndex);

    m_channels.push_back(channel);
    iChannelIndex++;
  }

  if (m_channels.size() == 0)
  {
    XBMC->Log(LOG_ERROR, "Unable to load channels - response parsed but no channels?");
    return false;
  }

  ApplyChannelsLogos();

  XBMC->Log(LOG_NOTICE, "Loaded %d channels.", m_channels.size());
  return true;
}

void PVRIptvData::ReloadEPG()
{
  //TODO reimplement if necessary
}

bool PVRIptvData::LoadEPGForChannel(const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  if (m_epg.size() == 0)
  {
    XBMC->Log(LOG_ERROR, "EPG channels not found.");
    return false;
  }

  std::string strEpgFromUrl("");
  std::string strRestUrl = m_strEpgRestUrl;
  strRestUrl += "?channel_number=" + std::to_string(channel.iChannelNumber);
  strRestUrl += "&start_time=" + std::to_string(iStart);
  strRestUrl += "&end_time=" + std::to_string(iEnd);
  strEpgFromUrl = grabEpg(strRestUrl);

  rapidjson::Document doc;
  doc.Parse(strEpgFromUrl.c_str());

  if (!doc.IsArray())
  {
    XBMC->Log(LOG_ERROR, "Cannot load epg - Invalid json format");
    return false;
  }

  PVRIptvEpgChannel *epg = NULL;
  if ((epg = FindEpg(std::to_string(channel.iChannelNumber))) == NULL)
  {
    XBMC->Log(LOG_ERROR, "Cannot find epg channel");
    return false;
  }

  int iBroadCastId = 0;
  for (auto& event : doc.GetArray())
  {
    PVRIptvEpgEntry entry;
    entry.iBroadcastId = ++iBroadCastId;
    entry.iGenreType = 0;
    entry.iGenreSubType = 0;
    entry.strPlotOutline = "";
    entry.startTime = event["programStartTime"].IsInt() ? event["programStartTime"].GetInt() : 0;
    entry.endTime = event["programEndTime"].IsInt() ? event["programEndTime"].GetInt() : 0;

    entry.strTitle = event["programTitle"].IsString() ? event["programTitle"].GetString() : "";
    entry.strPlot = event["programDescLong"].IsString() ? event["programDescLong"].GetString() : "";
    entry.strGenreString = event["categories"].IsString() ? event["categories"].GetString() : "";
    entry.strIconPath = "";

    epg->epg.push_back(entry);
  }

  XBMC->Log(LOG_NOTICE, "EPG Loaded.");

  return true;
}

int PVRIptvData::GetChannelsAmount(void)
{
  return m_channels.size();
}

PVR_ERROR PVRIptvData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    PVRIptvChannel &channel = m_channels.at(iChannelPtr);
    if (channel.bRadio == bRadio)
    {
      PVR_CHANNEL xbmcChannel;
      memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

      xbmcChannel.iUniqueId         = channel.iUniqueId;
      xbmcChannel.bIsRadio          = channel.bRadio;
      xbmcChannel.iChannelNumber    = channel.iChannelNumber;
      strncpy(xbmcChannel.strChannelName, channel.strChannelName.c_str(), sizeof(xbmcChannel.strChannelName) - 1);
      xbmcChannel.iEncryptionSystem = channel.iEncryptionSystem;
      strncpy(xbmcChannel.strIconPath, channel.strLogoPath.c_str(), sizeof(xbmcChannel.strIconPath) - 1);
      xbmcChannel.bIsHidden         = false;

      PVR->TransferChannelEntry(handle, &xbmcChannel);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

bool PVRIptvData::GetChannel(const PVR_CHANNEL &channel, PVRIptvChannel &myChannel)
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    PVRIptvChannel &thisChannel = m_channels.at(iChannelPtr);
    if (thisChannel.iUniqueId == (int) channel.iUniqueId)
    {
      myChannel.iUniqueId         = thisChannel.iUniqueId;
      myChannel.bRadio            = thisChannel.bRadio;
      myChannel.iChannelNumber    = thisChannel.iChannelNumber;
      myChannel.iEncryptionSystem = thisChannel.iEncryptionSystem;
      myChannel.strChannelName    = thisChannel.strChannelName;
      myChannel.strLogoPath       = thisChannel.strLogoPath;
      myChannel.strStreamURL      = thisChannel.strStreamURL;

      return true;
    }
  }

  return false;
}

int PVRIptvData::GetChannelGroupsAmount(void)
{
  return m_groups.size();
}

PVR_ERROR PVRIptvData::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  std::vector<PVRIptvChannelGroup>::iterator it;
  for (it = m_groups.begin(); it != m_groups.end(); ++it)
  {
    if (it->bRadio == bRadio)
    {
      PVR_CHANNEL_GROUP xbmcGroup;
      memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));

      xbmcGroup.iPosition = 0;      /* not supported  */
      xbmcGroup.bIsRadio  = bRadio; /* is radio group */
      strncpy(xbmcGroup.strGroupName, it->strGroupName.c_str(), sizeof(xbmcGroup.strGroupName) - 1);

      PVR->TransferChannelGroup(handle, &xbmcGroup);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  PVRIptvChannelGroup *myGroup;
  if ((myGroup = FindGroup(group.strGroupName)) != NULL)
  {
    std::vector<int>::iterator it;
    for (it = myGroup->members.begin(); it != myGroup->members.end(); ++it)
    {
      if ((*it) < 0 || (*it) >= (int)m_channels.size())
        continue;

      PVRIptvChannel &channel = m_channels.at(*it);
      PVR_CHANNEL_GROUP_MEMBER xbmcGroupMember;
      memset(&xbmcGroupMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

      strncpy(xbmcGroupMember.strGroupName, group.strGroupName, sizeof(xbmcGroupMember.strGroupName) - 1);
      xbmcGroupMember.iChannelUniqueId = channel.iUniqueId;
      xbmcGroupMember.iChannelNumber   = channel.iChannelNumber;

      PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRIptvData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  std::vector<PVRIptvChannel>::iterator myChannel;
  for (myChannel = m_channels.begin(); myChannel < m_channels.end(); ++myChannel)
  {
    if (myChannel->iUniqueId != (int) channel.iUniqueId)
      continue;

    //if (iStart > m_iLastStart || iEnd > m_iLastEnd)
    //{
      // reload EPG for new time interval only
      LoadEPGForChannel(channel, iStart, iEnd);
      {
        // doesn't matter is epg loaded or not we shouldn't try to load it for same interval
        m_iLastStart = iStart;
        m_iLastEnd = iEnd;
      }
    //}

    PVRIptvEpgChannel *epg;
    if ((epg = FindEpgForChannel(*myChannel)) == NULL || epg->epg.size() == 0)
      return PVR_ERROR_NO_ERROR;

    int iShift = m_bTSOverride ? m_iEPGTimeShift : myChannel->iTvgShift + m_iEPGTimeShift;

    std::vector<PVRIptvEpgEntry>::iterator myTag;
    for (myTag = epg->epg.begin(); myTag < epg->epg.end(); ++myTag)
    {
      if ((myTag->endTime + iShift) < iStart)
        continue;

      int iGenreType, iGenreSubType;

      EPG_TAG tag;
      memset(&tag, 0, sizeof(EPG_TAG));

      tag.iUniqueBroadcastId  = myTag->iBroadcastId;
      tag.strTitle            = myTag->strTitle.c_str();
      tag.iUniqueChannelId    = channel.iUniqueId;
      tag.startTime           = myTag->startTime + iShift;
      tag.endTime             = myTag->endTime + iShift;
      tag.strPlotOutline      = myTag->strPlotOutline.c_str();
      tag.strPlot             = myTag->strPlot.c_str();
      tag.strOriginalTitle    = NULL;  /* not supported */
      tag.strCast             = NULL;  /* not supported */
      tag.strDirector         = NULL;  /* not supported */
      tag.strWriter           = NULL;  /* not supported */
      tag.iYear               = 0;     /* not supported */
      tag.strIMDBNumber       = NULL;  /* not supported */
      tag.strIconPath         = myTag->strIconPath.c_str();
      tag.iGenreType          = EPG_GENRE_USE_STRING;
      tag.iGenreSubType       = 0;     /* not supported */
      tag.strGenreDescription = myTag->strGenreString.c_str();
      tag.iParentalRating     = 0;     /* not supported */
      tag.iStarRating         = 0;     /* not supported */
      tag.bNotify             = false; /* not supported */
      tag.iSeriesNumber       = 0;     /* not supported */
      tag.iEpisodeNumber      = 0;     /* not supported */
      tag.iEpisodePartNumber  = 0;     /* not supported */
      tag.strEpisodeName      = NULL;  /* not supported */
      tag.iFlags              = EPG_TAG_FLAG_UNDEFINED;

      PVR->TransferEpgEntry(handle, &tag);

      if ((myTag->startTime + iShift) > iEnd)
        break;
    }

    return PVR_ERROR_NO_ERROR;
  }

  return PVR_ERROR_NO_ERROR;
}

PVRIptvChannel * PVRIptvData::FindChannel(const std::string &strId, const std::string &strName)
{
  std::string strTvgName = strName;
  StringUtils::Replace(strTvgName, ' ', '_');

  std::vector<PVRIptvChannel>::iterator it;
  for(it = m_channels.begin(); it < m_channels.end(); ++it)
  {
    if (it->strTvgId == strId)
      return &*it;

    if (strTvgName == "")
      continue;

    if (it->strTvgName == strTvgName)
      return &*it;

    if (it->strChannelName == strName)
      return &*it;
  }

  return NULL;
}

PVRIptvChannelGroup * PVRIptvData::FindGroup(const std::string &strName)
{
  std::vector<PVRIptvChannelGroup>::iterator it;
  for(it = m_groups.begin(); it < m_groups.end(); ++it)
  {
    if (it->strGroupName == strName)
      return &*it;
  }

  return NULL;
}

PVRIptvEpgChannel * PVRIptvData::FindEpg(const std::string &strId)
{
  std::vector<PVRIptvEpgChannel>::iterator it;
  for(it = m_epg.begin(); it < m_epg.end(); ++it)
  {
    if (StringUtils::CompareNoCase(it->strId, strId) == 0)
      return &*it;
  }

  return NULL;
}

PVRIptvEpgChannel * PVRIptvData::FindEpgForChannel(PVRIptvChannel &channel)
{
  std::vector<PVRIptvEpgChannel>::iterator it;
  for(it = m_epg.begin(); it < m_epg.end(); ++it)
  {
    if (it->strId == channel.strTvgId)
      return &*it;

    std::string strName = it->strName;
    StringUtils::Replace(strName, ' ', '_');
    if (strName == channel.strTvgName
      || it->strName == channel.strTvgName)
      return &*it;

    if (it->strName == channel.strChannelName)
      return &*it;
  }

  return NULL;
}

void PVRIptvData::ApplyChannelsLogos()
{
  std::vector<PVRIptvChannel>::iterator channel;
  for(channel = m_channels.begin(); channel < m_channels.end(); ++channel)
  {
    if (!channel->strTvgLogo.empty())
    {
      if (!m_strLogoPath.empty()
        // special proto
        && channel->strTvgLogo.find("://") == std::string::npos)
        channel->strLogoPath = PathCombine(m_strLogoPath, channel->strTvgLogo);
      else
        channel->strLogoPath = channel->strTvgLogo;
    }
  }
}
