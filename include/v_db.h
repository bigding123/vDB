#pragma once

#include <string>
#include <sys/uio.h>
#include <functional>

namespace vDB {

/*
 * db_store�ĺϷ���־
 * INSERT����
 * REPLACE�滻
 * STORE�滻�����
 */
enum DB_STORE_FLAG{STORE_MIN_FLAG, DB_INSERT, DB_REPLACE, DB_STORE, STORE_MAX_FLAG};

const int kIndex_min = 6;     //index������СΪ6��key��sep��start��sep��length��\n
const int kIndex_max = 1024;  //index��󳤶ȣ���������Լ�����
const int kData_min = 2;      //data����С����Ϊ2��һ���ֽڼ�һ�����з�
const int kData_max = 1024;   //data����󳤶ȣ������Լ�����

using std::string;

typedef unsigned int DBHASH;       //hashֵ����

/*
 * һ��key->value���ݿ⣬���ݿ�򿪺�����������ļ�.idx��.dat�ļ�
 * .idx�洢key��������ص���Ϣ��.dat�洢����������
 * key��value��Ϊstring����
 * ע�⣬���������lseek���������Զ�ͬһ��������˵��Щ�ӿڶ��ǲ��������
 * key�������:����
 */
class DB {
public:
	explicit DB();
	DB(const DB&) = delete;
	virtual ~DB();
	/*
	 * �򿪻��ߴ������ݿ⣬������openϵͳ����һ��
	 * �ɹ�����Trueʧ�ܷ���False
	 */
	virtual bool db_open(const string&, int, ...);
	/*
	 * ���������ݿ�ķ���
	 */
	virtual void db_close();
	/*
	 * ��key��ȡ��Ӧ��value���������򷵻ؿ�string
	 */
	virtual string db_fetch(const string&);
	/*
	 * ɾ��ָ��key�ļ�¼
	 * �ɹ�����trueʧ�ܷ���false
	 */
	virtual bool db_delete(const string&);
	/*
	 * �Ѽ�¼�洢�����ݿ���
	 * ��һ��������key���ڶ���������value���������ǲ����ı�־
	 * �ɹ�����0�����󷵻�-1��������ڶ���ָ����DB_INSERT�򷵻�1
	 */
	virtual int db_store(const string&, const string&, int);
private:
	string pathname_;          //���ݿ�·��
	//���һ�ζ�дindex��¼ʱ��ǰһ���ڵ�ͺ�һ���ڵ��ƫ����
	off_t pre_offset_, next_offset_;
	//db_store��ͬflag��Ӧ��ӳ�亯��
	std::function<int(const string&, const string&, bool, off_t)> store_function_map[STORE_MAX_FLAG];

	struct Handle {
		int fd;                //�ļ�������
		off_t offset;          //���һ�ζ�д��¼ʱ��ƫ����
		int length;            //���һ�ζ�д�ļ�¼����
		char *buffer;          //��д��¼ʱ�õĻ�����
	}index_, data_;            //idx�ļ���dat�ļ�

	void _db_bind_function();
	bool _db_allocate();
	void _db_free();
	DBHASH _db_hash(const string&);
	bool _db_find(const string&, off_t);
	off_t _db_read_ptr(off_t);
	off_t _db_read_idx(off_t);
	char *_db_read_data();
	bool _db_do_delete();
	bool _db_write_data(const char*, off_t, int);
	bool _db_lock_and_write_data(const char*, off_t, int);
	bool _db_write_idx(const char*, off_t, int, off_t);
	bool _db_lock_and_write_idx(const char*, off_t, int, off_t);
	bool _db_pre_write_idx(const char*, off_t, struct iovec*, char*);
	bool _db_do_write_idx(off_t, int, struct iovec*);
	bool _db_write_ptr(off_t, off_t);
	int _db_store_insert(const string&, const string&, bool, off_t);
	int _db_store_replace(const string&, const string&, bool, off_t);
	int _db_store_ins_or_rep(const string&, const string&, bool, off_t);
	bool _db_find_and_delete_free(int, int);
};

}
