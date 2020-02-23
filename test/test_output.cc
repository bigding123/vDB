#include "../include/v_db.h"
#include <cstdio>
#include <string>
#include <functional>
#include <unordered_map>
#include <fcntl.h>
#include <iostream>

template<typename T>
bool check_result(T result1, T result2, int cmd_number, int cmd) {
	if (result1 != result2){
		printf("result is not same, cmd_number=%d, cmd=%d\n", cmd_number, cmd);
		std::cout<<"result1="<<result1<<" result2="<<result2<<std::endl;
		return false;
	}
	return true;
}

void test_output() {
	vDB::DB db;
	std::unordered_map<std::string, std::string> m;
	db.db_open("testdb", O_RDWR|O_CREAT|O_TRUNC);
	/*
	 * ͨ��input�ļ�����������ͬʱ������db��unorder_map
	 * �Ա����ǵ�����Ƿ�һ��
	 */
	freopen("input", "r", stdin);
	int cmd;
	int cmd_number = 1;       //��ʾ���ǵڼ��е�����
	while (~scanf("%d", &cmd)) {
		printf("cmd =%d, cmd_number=%d\n",cmd, cmd_number);
		if (!cmd) {
			char key[4], value[5];
			int flag;
			scanf("%s%s%d", key, value, &flag);
			printf("key=%s,value=%s,flag=%d\n",key,value,flag);
			int db_result, map_result;
			/*
			 * store����
			 * ����map��˵��Ҫ�Լ��ֶ���
			 */
			db_result = db.db_store(key, value, flag);
			if (m.find(key) == m.end()) {
				if (flag != 2){
					//����replace����
					m[key] = value;
					map_result = 0;
				}
				else
					map_result = -1;
			}
			else{
				if (flag != 1){
					//����insert����
					m[key] = value;
					map_result = 0;
				}
				else if(1 == flag)
					map_result = 1;
			}
			if (!check_result<int>(db_result, map_result, cmd_number, cmd))
				return;
		}
		else {
			char key[10];
			scanf("%s", key);
			printf("key=%s\n",key);
			if (1 == cmd) {
				/*
				 * delete����
				 * ����map��˵��Ҫ��һ���Ƿ�������key
				 */
				bool db_result, map_result;
				db_result = db.db_delete(key);
				auto element = m.find(key);
				if (element == m.end())
					map_result = 0;
				else {
					m.erase(element);
					map_result = 1;
				}
				if (!check_result<bool>(db_result, map_result, cmd_number, cmd))
					return;
			}
			else {
				/*
				 * fetch����
				 * ͬ����Ҫ�����Ƿ����
				 */
				std::string db_result, map_result;
				db_result = db.db_fetch(key);
				if (m.find(key) != m.end())
					map_result = m[key];
				if (!check_result<std::string>(db_result, map_result, cmd_number, cmd))
					return;
			}
		}
		cmd_number++;
	}
	db.db_close();
}

int main() {
	test_output();
}
