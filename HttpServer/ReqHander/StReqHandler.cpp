#include "StReqHandler.h"

void StReqHandler::ParseTimeString(std::string_view str_v, Json::Value& timeArr)
{
	do
	{
		Json::Value timeObj; // һ�� day-time ��

		auto c = str_v.find(" ");
		std::string_view day(str_v.data(), c);
		timeObj["day"] = std::string(day);
		str_v = str_v.substr(c + 1);
		c = str_v.find(",");
		if (c != std::string_view::npos) // ���滹��ʱ��
		{
			std::string_view time(str_v.data(), c);
			timeObj["time"] = std::string(time);
			str_v = str_v.substr(c + 1);
		}
		else
		{
			timeObj["time"] = std::string(str_v);
			return;
		}
		timeArr.append(timeObj);
	} while (str_v.size() > 0);
}