/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license, you may redistribute it
 * and/or modify it under version 3 of the License, or (at your option), any later version.
 */

#include "PlayerbotRepository.h"
#include "AiObjectContext.h"

void PlayerbotRepository::Load(PlayerbotAI* botAI)
{
    ObjectGuid::LowType guid = botAI->GetBot()->GetGUID().GetCounter();

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_DB_STORE);
    stmt->SetData(0, guid);
    if (PreparedQueryResult result = PlayerbotsDatabase.Query(stmt))
    {
        std::vector<std::string> values;
        do
        {
            Field* fields = result->Fetch();
            std::string const key = fields[0].Get<std::string>();
            std::string const value = fields[1].Get<std::string>();

            if (key == "value")
                values.push_back(value);
            else if (key == "co")
            {
                botAI->ClearStrategies(BOT_STATE_COMBAT);
                botAI->ChangeStrategy("+chat", BOT_STATE_COMBAT);
                botAI->ChangeStrategy(value, BOT_STATE_COMBAT);
            }
            else if (key == "nc")
            {
                botAI->ClearStrategies(BOT_STATE_NON_COMBAT);
                botAI->ChangeStrategy("+chat", BOT_STATE_NON_COMBAT);
                botAI->ChangeStrategy(value, BOT_STATE_NON_COMBAT);
            }
            else if (key == "dead")
                botAI->ChangeStrategy(value, BOT_STATE_DEAD);
        } while (result->NextRow());

        botAI->GetAiObjectContext()->GetUntypedValue("outfit list");

        botAI->GetAiObjectContext()->Load(values);
    }
}

void PlayerbotRepository::Save(PlayerbotAI* botAI)
{
    ObjectGuid::LowType guid = botAI->GetBot()->GetGUID().GetCounter();

    Reset(botAI);

    std::vector<std::string> data = botAI->GetAiObjectContext()->Save();

    std::string coStr = FormatStrategies("co", botAI->GetStrategies(BOT_STATE_COMBAT));
    std::string ncStr = FormatStrategies("nc", botAI->GetStrategies(BOT_STATE_NON_COMBAT));
    std::string deadStr = FormatStrategies("dead", botAI->GetStrategies(BOT_STATE_DEAD));

    // Build a single batch INSERT to replace per-row SaveValue calls.
    // This eliminates N individual INSERT statements per bot save,
    // dramatically reducing DB round-trips for bots with many AI context values.
    std::ostringstream sql;
    sql << "INSERT INTO `playerbots_db_store` (`guid`, `key`, `value`) VALUES ";

    bool first = true;
    auto appendRow = [&](std::string const& key, std::string const& value)
    {
        if (value.empty())
            return;

        if (!first)
            sql << ", ";
        first = false;

        std::string escapedValue = value;
        PlayerbotsDatabase.EscapeString(escapedValue);
        sql << "(" << guid << ", '" << key << "', '" << escapedValue << "')";
    };

    for (auto const& value : data)
        appendRow("value", value);

    appendRow("co", coStr);
    appendRow("nc", ncStr);
    appendRow("dead", deadStr);

    if (!first)
        PlayerbotsDatabase.Execute(sql.str().c_str());
}

std::string const PlayerbotRepository::FormatStrategies(std::string const /*type*/, std::vector<std::string> strategies)
{
    std::ostringstream out;
    for (std::vector<std::string>::iterator i = strategies.begin(); i != strategies.end(); ++i)
        out << "+" << (*i).c_str() << ",";

    std::string const res = out.str();
    return res.substr(0, res.size() - 1);
}

void PlayerbotRepository::Reset(PlayerbotAI* botAI)
{
    ObjectGuid::LowType guid = botAI->GetBot()->GetGUID().GetCounter();

    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_DEL_DB_STORE);
    stmt->SetData(0, guid);
    PlayerbotsDatabase.Execute(stmt);
}

void PlayerbotRepository::SaveValue(uint32 guid, std::string const key, std::string const value)
{
    PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_INS_DB_STORE);
    stmt->SetData(0, guid);
    stmt->SetData(1, key);
    stmt->SetData(2, value);
    PlayerbotsDatabase.Execute(stmt);
}
