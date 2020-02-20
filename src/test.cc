#include "v_db.h"
#include <cstdio>
#include <string>
#include <functional>
#include <unordered_map>

void generate_input() {
	//���ɲ�������
	freopen("input", "w", stdout);
	const int kCmd_number = 10000;   //���ɵ�������
	const int kKey_max = 1000;       //key�����ֵ
	const int kValue_max = 10000;    //value�����ֵ
	srand((unsigned)time(NULL));
	for (int i = 0; i < kCmd_number; ++i) {
		int cmd = rand() % 3;
		printf("%d", cmd);
		/*
		 * 0 store
		 * 1 delete
		 * 2 fetch
		 */
		if (!cmd) {
			int key = rand() % kKey_max;
			int value = rand() % kValue_max;
			int flag = rand() % 3 + 1;//��Χ��1-3֮��
			printf("%d %d %d\n", key, value, flag);
		}
		else {
			int key = rand() % kKey_max;
			printf("%d\n", key);
		}
	}
}

template<typename T>
void check_result(T result1, T result2) {
	if (result1 != result2)
		printf("result is not same\n");
}

void test_output() {
	vDB::DB db;
	std::unordered_map<std::string, std::string> m;
	//db.db_open()
	/*
	 * ͨ��input�ļ�����������ͬʱ������db��unorder_map
	 * �Ա����ǵ�����Ƿ�һ��
	 */
	freopen("input", "r", stdin);
	int cmd;
	while (~scanf("%d", &cmd)) {
		if (!cmd) {
			char key[4], value[5];
			int flag;
			scanf("%s%s%d", &key, value, flag);
			int db_result, map_result;
			/*
			 * store����
			 * ����map��˵��Ҫ�Լ��ֶ���
			 */
			db_result = db.db_store(key, value, flag);
			if (m.find(key) == m.end()) {
				m[key] = value;
				map_result = 0;
			}
			else
				map_result = 1;
			check_result<int>(db_result, map_result);
		}
		else {
			char key[4];
			scanf("%s", &key);
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
				check_result<bool>(db_result, map_result);
			}
			else {
				/*
				 * fetch����
				 * ͬ����Ҫ�����Ƿ����
				 */
				std::string db_result, map_result;
				db_result = db.db_fetch(key);
				if (m.find(key) == m.end())
					map_result = m[key];
				check_result<std::string>(db_result, map_result);
			}
		}
	}
	//db_close
}

int main() {
	
}