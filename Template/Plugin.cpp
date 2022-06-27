#include "pch.h"
#include <EventAPI.h>
#include <LoggerAPI.h>
#include <MC/Level.hpp>
#include <MC/BlockInstance.hpp>
#include <MC/Block.hpp>
#include <MC/BlockSource.hpp>
#include <MC/Actor.hpp>
#include <MC/Mob.hpp>
#include <MC/Player.hpp>
#include <MC/ItemStack.hpp>
#include "Version.h"
#include <LLAPI.h>
#include <ServerAPI.h>
#include <MC/Json.hpp>
#include <io.h>
#include <direct.h>
#include <time.h>

Logger PlayerKBLogger(PLUGIN_NAME);

inline void CheckProtocolVersion() {
#ifdef TARGET_BDS_PROTOCOL_VERSION
    auto currentProtocol = LL::getServerProtocolVersion();
    if (TARGET_BDS_PROTOCOL_VERSION != currentProtocol)
    {
        logger.warn("Protocol version not match, target version: {}, current version: {}.",
            TARGET_BDS_PROTOCOL_VERSION, currentProtocol);
        logger.warn("This will most likely crash the server, please use the Plugin that matches the BDS version!");
    }
#endif // TARGET_BDS_PROTOCOL_VERSION
}

using namespace std;

unordered_map<HMENU, int> playerhash;
json config = R"(
  {
	"excludeOP" : false,
    "kb_player_player":
		{
			"enable" : true,
			"maxkb"  : 3.0,
			"coefficient" : 1.0
		},
	"kb_player_actor":
		{
			"enable" : true,
			"maxkb"	 : 3.0,
			"coefficient" : 1.0
		},
    "attack":
		{
			"enable" : true,
			"only_player" : false,
			"time_ms": 10
		}	
  }
)"_json;

string configpath = "./plugins/PlayerKB/";


string UtfToGbk(const char* utf8);

void PluginInit()
{
	CheckProtocolVersion();

	if (_access(configpath.c_str(), 0) == -1)	//表示配置文件所在的文件夹不存在
	{
		if (_mkdir(configpath.c_str()) == -1)
		{
			//文件夹创建失败
			PlayerKBLogger.warn("Directory creation failed, please manually create the plugins/PlayerKB directory");
			return;
		}
	}
	ifstream f((configpath + "PlayerKB.json").c_str());
	if (f.good())								//表示配置文件存在
	{
		string txt = "";
		string s;
		while (getline(f, s))
		{
			txt += s + "\n";
		}
		config = json::parse(txt);
		f.close();
	}
	else {
		//配置文件不存在
		ofstream c((configpath + "PlayerKB.json").c_str());
		c << config.dump(2);
		c.close();
	}

	ifstream t(UtfToGbk((configpath + "配置说明.txt").c_str()));
	if (!t.good())  // 如果配置文件的说明文件不存在
	{
		ofstream c(UtfToGbk((configpath + "配置说明.txt").c_str()));
		c << "excludeOP : 此插件的限制是否排除管理员 默认(布尔值:false)\n";
		c << "kb_player_player : 玩家击退玩家部分的配置 默认(JSON对象)\n";
		c << "\tenable : 是否开启这部分功能 默认(布尔值:true)\n";
		c << "\tmaxkb : 最大击退力度 大于 x 则等于 x ,配置值小于等于0则不处理 默认(float:3.0)\n";
		c << "\tcoefficient : 击退力度系数 最大力度计算后的值将会乘以该系数 默认(float:1.0)\n";
		c << "kb_player_actor : 玩家击退生物部分的配置 默认(JSON对象)\n";
		c << "\tenable : 是否开启这部分功能 默认(布尔值:true)\n";
		c << "\tmaxkb : 最大击退力度 大于 x 则等于 x ,配置值小于等于0则不处理 默认(float:3.0)\n";
		c << "\tcoefficient : 击退力度系数 最大力度计算后的值将会乘以该系数 默认(float:1.0)\n";
		c << "attack : 玩家攻击间隔部分的配置 默认(JSON对象)\n";
		c << "\tenable : 是否开启这部分功能 默认(布尔值:true)\n";
		c << "\tonly_player : 是否只处理被攻击生物仅是玩家的情况false表示玩家对所有生物有效 默认(布尔值:false)\n";
		c << "\ttime_ms : 攻击间隔需要大于这个时间,否则攻击无效 单位 ms 默认(int:10)\n";
		c.close();
	}

}

//JSON 内容
//击退适用于所有生物对玩家 包括玩家对玩家 锁定(暂时只是对玩家有效)，JSON里面没有
//float 击退系数(原来的击退力度 * 该系数)
//int 攻击间隔(ms)，上次攻击离这次攻击之间的时间 小于这个数则攻击不起作用
//bool 攻击间隔是否是玩家对所有生物有效(true),还是玩家只对玩家有效(false)



// Hook 生物击退
/*
* thi	  : 被击退的生物
* actor	  : 产生击退的生物
* b0	  : 被击退生物(玩家)的朝向,对击退不影响1->X- 2->Z- 3->X+ 4->Z+
* b1	  : 增大是往X- 方向击退 (自己的箭射中自己时 值为0)
* b2	  : 增大是往Z- 方向击退 (自己的箭射中自己时 值为0)
* b3	  : 平面方向上的击退力度 (自己的箭射中自己时 值为1 冲击1-> 1.6 冲击2->3.2)
* b4	  : 未知(0.4)
* b5	  : 未知(0.4)
 */
THook(void, "?knockback@Mob@@UEAAXPEAVActor@@HMMMMM@Z", Mob* thi, Actor* actor, float b0, float b1, float b2, float b3, float b4, float b5)
{
	//如果攻击者不是玩家
	if (!actor->isPlayer())
	{
		original(thi, actor, b0, b1, b2, b3, b4, b5);
		return;
	}
	if (config["excludeOP"])
	{
		if (((Player*)actor)->isOP())
		{
			original(thi, actor, b0, b1, b2, b3, b4, b5);
			return;
		}
		if (thi->isPlayer() && ((Player*)thi)->isOP())
		{
			original(thi, actor, b0, b1, b2, b3, b4, b5);
			return;
		}
	}


	if (thi->isPlayer())	//这里内部表示  是玩家对玩家的事件
	{
		if (!config["kb_player_player"]["enable"])
		{
			//玩家对玩家 的配置选项没有打开 则不额外处理 直接返回
			original(thi, actor, b0, b1, b2, b3, b4, b5);
			return;
		}
		if (config["kb_player_player"]["maxkb"] > 0 && b3 > config["kb_player_player"]["maxkb"])
		{
			b3 = config["kb_player_player"]["maxkb"];
		}
		b3 = b3 * config["kb_player_player"]["coefficient"];	//处理 攻击力度系数
		original(thi, actor, b0, b1, b2, b3, b4, b5);
		return;
	}

	// 执行到这里这里表示 事件是玩家对生物 不包括玩家对玩家
	if (!config["kb_player_actor"]["enable"])
	{
		//玩家对生物的配置选项打开
		original(thi, actor, b0, b1, b2, b3, b4, b5);
		return;
	}

	if (config["kb_player_actor"]["maxkb"] > 0 && b3 > config["kb_player_actor"]["maxkb"])
	{
		b3 = config["kb_player_actor"]["maxkb"];
	}
	b3 = b3 * config["kb_player_actor"]["coefficient"];	//处理 攻击力度系数
	original(thi, actor, b0, b1, b2, b3, b4, b5);
	return;
}

/*
* player	: 产生攻击的玩家
* actor		: 玩家攻击的生物
* adc		: 攻击类型
*/
THook(char, "?attack@Player@@UEAA_NAEAVActor@@AEBW4ActorDamageCause@@@Z", Player* player, Actor* actor, enum ActorDamageCause* adc)
{
	if (!config["attack"]["enable"] || (config["excludeOP"] && player->isOP())) {
		return original(player, actor, adc);
	}
	if (!actor->isPlayer() && config["attack"]["only_player"]) {
		return original(player, actor, adc);
	}
	int lasttime = playerhash[(HMENU)player];
	if (lasttime == NULL) {
		playerhash[(HMENU)player] = clock();
		return original(player, actor, adc);
	}
	else {
		int nowtime = clock();
		if (nowtime - lasttime < config["attack"]["time_ms"]) {
			return 0;
		}
		playerhash[(HMENU)player] = nowtime;
		return original(player, actor, adc);
	}
}

string UtfToGbk(const char* utf8)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1];
	memset(wstr, 0, len + 1);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, len);
	len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	char* str = new char[len + 1];
	memset(str, 0, len + 1);
	WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, len, NULL, NULL);
	if (wstr) delete[] wstr;
	return str;
}