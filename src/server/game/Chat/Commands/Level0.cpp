/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Common.h"
#include "World.h"
#include "Player.h"
#include "Chat.h"
#include "ObjectAccessor.h"
#include "Language.h"
#include "AccountMgr.h"
#include "SystemConfig.h"
#include "revision.h"
#include "Util.h"
#include "Group.h"

bool ChatHandler::HandleHelpCommand(const char* args)
{
    char* cmd = strtok((char*)args, " ");
    if (!cmd)
    {
        ShowHelpForCommand(getCommandTable(), "help");
        ShowHelpForCommand(getCommandTable(), "");
    }
    else
    {
        if (!ShowHelpForCommand(getCommandTable(), cmd))
            SendSysMessage(LANG_NO_HELP_CMD);
    }

    return true;
}

bool ChatHandler::HandleCommandsCommand(const char* /*args*/)
{
    ShowHelpForCommand(getCommandTable(), "");
    return true;
}

bool ChatHandler::HandleStartCommand(const char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    if (player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        SetSentErrorMessage(true);
        return false;
    }

    if (player->isInCombat())
    {
        SendSysMessage(LANG_YOU_IN_COMBAT);
        SetSentErrorMessage(true);
        return false;
    }

    if (player->isDead() || player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        // if player is dead and stuck, send ghost to graveyard
        player->RepopAtGraveyard();
        return true;
    }

    // cast spell Stuck
    player->CastSpell(player, 7355, false);
    return true;
}

bool ChatHandler::HandleDismountCommand(const char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    //If player is not mounted, so go out :)
    if (!player->IsMounted())
    {
        SendSysMessage(LANG_CHAR_NON_MOUNTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        SetSentErrorMessage(true);
        return false;
    }

    player->Dismount();
    player->RemoveAurasByType(SPELL_AURA_MOUNTED);
    return true;
}

bool ChatHandler::HandleSaveCommand(const char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    // save GM account without delay and output message
    if (!AccountMgr::IsPlayerAccount(m_session->GetSecurity()))
    {
        if (Player* target = getSelectedPlayer())
            target->SaveToDB();
        else
            player->SaveToDB();
        SendSysMessage(LANG_PLAYER_SAVED);
        return true;
    }

    // save if the player has last been saved over 20 seconds ago
    uint32 save_interval = sWorld->getIntConfig(CONFIG_INTERVAL_SAVE);
    if (save_interval == 0 || (save_interval > 20 * IN_MILLISECONDS && player->GetSaveTimer() <= save_interval - 20 * IN_MILLISECONDS))
        player->SaveToDB();

    return true;
}

//Playerbot mod
bool ChatHandler::HandlePlayerbotCommand(const char *args)
{
    if(!m_session)
    {
        PSendSysMessage("You may only add bots from an active session");
        SetSentErrorMessage(true);
        return false;
    }

    if(!*args)
    {
        PSendSysMessage("usage: add PLAYERNAME  or  remove PLAYERNAME");
        SetSentErrorMessage(true);
        return false;
    }

    char *cmd = strtok ((char*)args, " ");
    char *charname = strtok (NULL, " ");
    if(!cmd || !charname)
    {
        PSendSysMessage("usage: add PLAYERNAME  or  remove PLAYERNAME");
        SetSentErrorMessage(true);
        return false;
    }

    std::string cmdStr = cmd;
    std::string charnameStr = charname;
    uint64 guid;

   if (charnameStr.compare("all") != 0)
   {
       if (!normalizePlayerName(charnameStr))
           return false;

       guid = sObjectMgr->GetPlayerGUIDByName(charnameStr.c_str());
       if (guid == 0 || (guid == m_session->GetPlayer()->GetGUID()))
       {
           SendSysMessage(LANG_PLAYER_NOT_FOUND);
           SetSentErrorMessage(true);
           return false;
       }

       uint32 accountId = sObjectMgr->GetPlayerAccountIdByGUID(guid);
       if (accountId != m_session->GetAccountId())
       {
           PSendSysMessage("You may only add bots from the same account.");
           SetSentErrorMessage(true);
           return false;
       }
   }

    if (cmdStr.compare("add") == 0 || cmdStr.compare("login") == 0)
    {
        if (charnameStr.compare("all") == 0)
        {
            std::list<std::string> *names;
            names=m_session->GetPlayer()->GetCharacterList();
            std::list<std::string>::iterator iter,next;
            for (iter = names->begin(); iter != names->end(); iter++)
            {
                std::stringstream arg;
                arg << "add " << (*iter).c_str();
                HandlePlayerbotCommand(arg.str().c_str());
            }
            PSendSysMessage("Bots added successfully.");
            return true;
        }
        else
        {
            if(m_session->GetPlayerBot(guid) != NULL)
            {
                PSendSysMessage("Bot already exists in world.");
                SetSentErrorMessage(true);
                return false;
            }
            m_session->AddPlayerBot(guid);
        }

    }
    else if (cmdStr.compare("remove") == 0 || cmdStr.compare("logout") == 0)
    {
        if (charnameStr.compare("all") == 0)
        {
            std::list<std::string> *names = new std::list<std::string>;
            for (PlayerBotMap::const_iterator iter = m_session->GetPlayerBotsBegin(); iter != m_session->GetPlayerBotsEnd(); ++iter)
            {
                names->push_back(iter->second->GetName());
            }
            std::list<std::string>::iterator iter,next;
            for (iter = names->begin(); iter != names->end(); iter++)
            {
                std::stringstream arg;
                arg << "remove " << (*iter).c_str();
                HandlePlayerbotCommand(arg.str().c_str());
            }
            return true;
        }
        else
        {
            if (m_session->GetPlayerBot(guid) == NULL)
            {
                PSendSysMessage("Bot can not be removed because bot does not exist in world.");
                SetSentErrorMessage(true);
                return false;
            }
            m_session->LogoutPlayerBot(guid, true);
            PSendSysMessage("Bot removed successfully.");
            return true;
        }
    }
    return true;
}

bool ChatHandler::HandlePlayerbotMainTankCommand(const char *args)
{
    uint64 guid = 0;
    uint64 pGuid = 0;
    char *charname ;
    Group *group = m_session->GetPlayer()->GetGroup();

    if (group == NULL) {
        PSendSysMessage("Must be in a group to set a main tank.");
        SetSentErrorMessage(true);
        return false;
    }

    QueryResult result = CharacterDatabase.PQuery("SELECT memberGuid FROM group_member WHERE memberFlags='%u' AND guid = '%u'",MEMBER_FLAG_MAINTANK, group->GetGUID());
    if(result)
    {
        pGuid = MAKE_NEW_GUID(result->Fetch()->GetInt32(),0,HIGHGUID_PLAYER);
    }

    // if no arguments are passed in, just say who the current main tank is
    if(!*args) {

        if (pGuid>0) {
            Player *pPlayer = ObjectAccessor::FindPlayer(pGuid);

            if (pPlayer  && pPlayer->isAlive()){
                PSendSysMessage("Main tank is %s.", pPlayer->GetName());
                return true;
            }
        }

        PSendSysMessage("Currently there is no main tank. ");
        return true;
    } else {
        charname = strtok ((char*)args, " ");
        std::string charnameStr = charname;
        guid = sObjectMgr->GetPlayerGUIDByName(charnameStr.c_str());

        // clear if same player
        if (pGuid==guid) {
            group->SetGroupMemberFlag(guid, false, MEMBER_FLAG_MAINTANK);
            PSendSysMessage("Main tank has been cleared. ");
            return true;
        }

        if (m_session->GetPlayer()->GetGroup()->IsMember(guid)) {
            group->SetGroupMemberFlag(pGuid,false, MEMBER_FLAG_MAINTANK); // clear old one
            group->SetGroupMemberFlag(guid, true, MEMBER_FLAG_MAINTANK);  // set new one
            Player *pPlayer = ObjectAccessor::FindPlayer(guid);
            if (pPlayer->IsInWorld())
                PSendSysMessage("Main tank is %s.", pPlayer->GetName());
            else
                PSendSysMessage("Player is not online.");

        } else {
            PSendSysMessage("Player is not in your group.");
        }

    }


    return true;
}

