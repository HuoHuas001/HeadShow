#include "pch.h"
#include <MC/Minecraft.hpp>
#include <MC/Actor.hpp>
#include <MC/Mob.hpp>
#include <MC/Player.hpp>
#include <MC/ServerPlayer.hpp>
#include <MC/Certificate.hpp>
#include <MC/CompoundTag.hpp>
#include <MC/NetworkHandler.hpp>
#include <MC/ServerNetworkHandler.hpp>
#include <MC/NetworkIdentifier.hpp>
#include <MC/NetworkPeer.hpp>
#include <MC/Level.hpp>
#include <RegCommandAPI.h>
#include <MC/IdentityDefinition.hpp>
#include "../SDK/Header/Utils/PlayerMap.h"
#include "../SDK/Header/third-party/Nlohmann/json.hpp"
#include <LoggerAPI.h>
#include <LLAPI.h>
#include <EventAPI.h>
#include <MC/Scoreboard.hpp>
#include <MC/Objective.hpp>
#include "money.h"
#include <ScheduleAPI.h>
//#include "../SDK/Header/LLMoney.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib/httplib.h>
#include "../Lib/PlaceholderAPI.h"
#include <MC/Attribute.hpp>
#include <MC/AttributeInstance.hpp>

typedef money_t(*LLMoneyGet_T)(xuid_t);
typedef string(*LLMoneyGetHist_T)(xuid_t, int);
typedef bool (*LLMoneyTrans_T)(xuid_t, xuid_t, money_t, string const&);
typedef bool (*LLMoneySet_T)(xuid_t, money_t);
typedef bool (*LLMoneyAdd_T)(xuid_t, money_t);
typedef bool (*LLMoneyReduce_T)(xuid_t, money_t);
typedef void (*LLMoneyClearHist_T)(int);

struct dynamicSymbolsMap_type
{
	LLMoneyGet_T LLMoneyGet = nullptr;
	LLMoneySet_T LLMoneySet = nullptr;
	LLMoneyAdd_T LLMoneyAdd = nullptr;
	LLMoneyReduce_T LLMoneyReduce = nullptr;
	LLMoneyTrans_T LLMoneyTrans = nullptr;
	LLMoneyGetHist_T LLMoneyGetHist = nullptr;
	LLMoneyClearHist_T LLMoneyClearHist = nullptr;
} dynamicSymbolsMap;

using nlohmann::json;
Logger logger("HeadShow");
using namespace std;
playerMap<string> ORIG_NAME;
int tick = 0;
bool ServerStarted = false;
const std::string ver = "v0.0.9";
const std::string fileName = "plugins/HeadShow/config.json";

//修改返回的Name 
THook(string&, "?getNameTag@Actor@@UEBAAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ", void* x) {
	if (auto it = ORIG_NAME._map.find((ServerPlayer*)x); it != ORIG_NAME._map.end()) {
		return it->second;
	}
	return original(x);
}

//Config
string defaultString = "%player_realname%\n§c❤§a%player_health%§e/§b%player_max_health% §a%player_hunger%§e/§b%player_max_hunger%\n§f%player_device% §c%player_ping%ms";
json defaultScoreBoard;
int defaultTick = 40;

nlohmann::json globaljson() {
	nlohmann::json json;
	json["updateTick"] = defaultTick;
	json["scoreBoard"] = defaultScoreBoard;
	json["showTitle"] = defaultString;
	return json;
}

void initjson(nlohmann::json json) {
	if (json.find("updateTick") != json.end()) {
		const nlohmann::json& out = json.at("updateTick");
		out.get_to(defaultTick);
	}
	if (json.find("scoreBoard") != json.end()) {
		const nlohmann::json& out = json.at("scoreBoard");
		out.get_to(defaultScoreBoard);
	}
	if (json.find("showTitle") != json.end()) {
		const nlohmann::json& out = json.at("showTitle");
		out.get_to(defaultString);
	}
}

void WriteDefaultData(const std::string& fileName) {
	//初始化scoreboard
	defaultScoreBoard["score"] = "money";
	defaultScoreBoard["score1"] = "money1";
	std::ofstream file(fileName);
	if (!file.is_open()) {
		logger.warn("Can't open config.json file(" + fileName + ")");
		return;
	}
	auto json = globaljson();
	file << json.dump(4);
	file.close();
}

void readJson() {
	std::ifstream file(fileName);
	if (!file.is_open()) {
		logger.warn("Can't open config.json file(" + fileName + ")");
		return;
	}
	nlohmann::json json;
	file >> json;
	file.close();
	initjson(json);
}

void loadConfig() {
	//data
	if (!std::filesystem::exists("plugins/HeadShow"))
		std::filesystem::create_directories("plugins/HeadShow");
	//tr	
	if (std::filesystem::exists(fileName)) {
		try {
			readJson();
		}
		catch (std::exception& e) {
			logger.error("Config File isInvalid, Err {}", e.what());
			Sleep(1000 * 100);
			exit(1);
		}
		catch (...) {
			logger.error("Config File isInvalid");
			Sleep(1000 * 100);
			exit(1);
		}
	}
	else {
		logger.info("Config File with default values created");
		WriteDefaultData(fileName);
	}
}

bool EconomySystem::init() {
	auto llmoney = LL::getPlugin("LLMoney");
	if (!llmoney)
	{
		logger.warn("The %money% variable will not take effect if LLMoney is not preceded");
		return false;
	}

	HMODULE h = llmoney->handler;
	dynamicSymbolsMap.LLMoneyGet = (LLMoneyGet_T)GetProcAddress(h, "LLMoneyGet");
	if (!dynamicSymbolsMap.LLMoneyGet)
		logger.warn("Fail to load API money.getMoney!");
	return true;
}

void version() {
	httplib::SSLClient cli("api.github.com", 443);
	if (auto res = cli.Get("/repos/HuoHuas001/HeadShow/releases/latest")) {
		if (res->status == 200){
			string body = res->body;
			json j = json::parse(body);

			//获取版本
			string getVersion = j.at("name");
			if (getVersion != ver) {
				//更新文件链接
				string downloadUrl = j["assets"][0]["browser_download_url"];
				logger.warn("New version update detected, download link:"+downloadUrl+".");
			}
			else {
				logger.info("Your version is already the latest version.");
			}
		}
		else {
			logger.warn("Failed to detect updates.");
		}
	}
	else {
		cout << "error code: " << res.error() << std::endl;
	}
}

string m_replace(string strSrc,
	const string& oldStr, const string& newStr, int count = -1)
{
	string strRet = strSrc;
	size_t pos = 0;
	int l_count = 0;
	if (-1 == count) // replace all
		count = strRet.size();
	while ((pos = strRet.find(oldStr, pos)) != string::npos)
	{
		strRet.replace(pos, oldStr.size(), newStr);
		if (++l_count >= count) break;
		pos += newStr.size();
	}
	return strRet;
}

class ReloadCommand : public Command {
public:
	void execute(CommandOrigin const& ori, CommandOutput& output) const override {//执行部分
		ServerPlayer* sp = ori.getPlayer();
		//检测玩家权限
		if (sp->isPlayer()) {
			if (sp->getPlayerPermissionLevel() > 0) {
				loadConfig();
				output.addMessage("HeadShow file reload success.");
				//检查计分板
				if (defaultScoreBoard.size() != 0) {
					for (auto it = defaultScoreBoard.begin(); it != defaultScoreBoard.end(); ++it) {
						string board = (string)it.value();
						if (!::Global<Scoreboard>->getObjective(board)) {
							auto obj = Scoreboard::newObjective(board, board);
							output.addMessage("Failed to find " + board + ", created automatically");
						}
					}
				}
			}
		}
	}

	static void setup(CommandRegistry* registry) {//注册部分(推荐做法)
		registry->registerCommand("reheadshow", "Reload HeadShow Config", CommandPermissionLevel::GameMasters, { (CommandFlagValue)0 }, { (CommandFlagValue)0x80 });
		registry->registerOverload<ReloadCommand>("reheadshow");
	}
};

money_t EconomySystem::getMoney(xuid_t player)
{
	if (dynamicSymbolsMap.LLMoneyGet)
		return dynamicSymbolsMap.LLMoneyGet(player);
	else
	{
		logger.error("API money.getMoney have not been loaded!");
		return 0;
	}
}

string forEachReplace(std::unordered_map<string, string> d, string s) {
	for (auto& i : d) {
		s = m_replace(s, i.first, i.second);
	}
	return s;
}


bool updateHead() {
	for (auto pl : Level::getAllPlayers())
	{
		try {
			string name = pl->getRealName();
			ORIG_NAME[(ServerPlayer*)pl] = name.c_str();
			std::unordered_map<string, string> ud = {};
			string dfs = defaultString;
			//获取scoreboard
			if (defaultScoreBoard.size() != 0) {
				for (auto it = defaultScoreBoard.begin(); it != defaultScoreBoard.end(); ++it) {
					string score = std::to_string(pl->getScore((string)it.value()));
					ud["%" + it.key() + "%"] = score;
				}
			}
			//获取Money
			if (dynamicSymbolsMap.LLMoneyGet) {
				string money = std::to_string((int)EconomySystem::getMoney(pl->getXuid()));
				ud["%money%"] = money;
			}
			//更改饥饿值
			float hunger = pl->getAttribute(Player::HUNGER).getCurrentValue();
			std::stringstream stream;
			stream << std::fixed << std::setprecision(1) << hunger;
			std::string hunger_str = stream.str();
			dfs = m_replace(dfs, "%player_hunger%", hunger_str);

			//格式化接入PAPI
			PlaceholderAPI::translateString(dfs, pl);
			string sinfo = forEachReplace(ud, dfs);
			//设置NameTag
			pl->rename(sinfo);
		}
		catch (...) {

		}
		
	}
	return true;
}

//玩家生命变动
THook(long long, "?change@HealthAttributeDelegate@@UEAAMMMAEBVAttributeBuff@@@Z", __int64 a1, float a2, float a3, __int64 a4) {
	Actor* ac = *(Actor**)(a1 + 32);
	if (ac->getTypeName() == "minecraft:player") {
		updateHead();
	}
	return original(a1, a2, a3, a4);
}


void PluginInit()
{
	LL::registerPlugin("HeadShow", "Show info on player's belowname", LL::Version(0, 0, 1));
	logger.info("HeadShow Loading...");
	//初始化llmoney的api
	EconomySystem::init();
	//读取json文件
	loadConfig();
	logger.info("HeadShow Loaded. By HuoHua");

	//注册指令
	Event::RegCmdEvent::subscribe([](Event::RegCmdEvent ev) { //注册指令事件
		ReloadCommand::setup(ev.mCommandRegistry);
		return true;
	});

	//检测开服
	Event::ServerStartedEvent::subscribe([](Event::ServerStartedEvent ev) {
		//检查更新
		version();
		//开始替换
		Schedule::repeat(updateHead, defaultTick);
		ServerStarted = true;
		//检查计分板
		if (defaultScoreBoard.size() != 0) {
			for (auto it = defaultScoreBoard.begin(); it != defaultScoreBoard.end(); ++it) {
				string board = (string)it.value();
				if (!::Global<Scoreboard>->getObjective(board)) {
					auto obj = Scoreboard::newObjective(board, board);
					logger.warn("Failed to find " + board + ", created automatically");
				}
			}
		}
		return true;
		});
}


