#include "../include/v_db.h"
#include "../include/record_lock.h"

#include <cstring>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/stat.h>

const int kPtr_size = 7;                 //idx�ļ��е�ptr�ṹ�Ĵ�С
const off_t kPtr_max = 9999999;          //ptr�����ֵ��7λ
const int kHash_table_size = 137;        //Ĭ�ϵ�hash���С
const off_t kHash_offset = kPtr_size;    //idx�ļ���hash���ƫ����
const int kHash_multipy_factor = 31;     //����hashֵʱ���۳�����
const int kIndex_length_size = 4;        //�洢index��¼���ȵ��ֽ���
const off_t kFree_offset = 0;            //��������ƫ����

const char kNew_line = '\n';             //���з�
const char kSeparate = ':';              //�ָ���
const char kSpace = ' ';                 //�ո��

namespace vDB {

DB::DB() {
	//��ʼ��ӳ�亯��
	_db_bind_function();
}
DB::~DB() {
	/*
	 * �뿪��������ͷ���Դ
	 * ��û����Լ�����close��
	 */
	_db_free();
}

bool DB::db_open(const string &pathname, int oflag, ...) {
	//���fpathname��������Ϊ��
	if (!pathname.length()){
		printf("db_open: pathname can not be blank\n");
		return false;
	}
	//������Դ
	if (!_db_allocate())
		return false;
	/*
	 * ��ʼ��·����fd
	 */
	pathname_ = pathname;
	index_.fd = data_.fd = -1;
	//����oflag
	if (oflag & O_CREAT) {
		va_list ap;
		va_start(ap, oflag);
		int mode = va_arg(ap, int);
		va_end(ap);
		
		//��idx�ļ���dat�ļ�
		index_.fd = open((pathname_ + ".idx").c_str(), oflag, mode);
		data_.fd = open((pathname_ + ".dat").c_str(), oflag, mode);
	}
	else {
		//��idx�ļ���dat�ļ�
		index_.fd = open((pathname_ + ".idx").c_str(), oflag);
		data_.fd = open((pathname_ + ".dat").c_str(), oflag);
	}
	if (index_.fd < 0 || data_.fd < 0) {
		//��ʧ��
		_db_free();
		return false;
	}
	if ((oflag & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC)) {
		/*
		 * ������ݿ������´����ģ����Ǳ����ʼ����
		 * д��ס�����ļ������ǲ���ͳ�������ҽ��г�ʼ��
		 * ������Ĵ�С�������Զ���ʼ����
		 */
		struct stat statbuff;
		char asciiptr[kPtr_size + 1], hash[(kHash_table_size + 1) * kPtr_size + 2];   //+2��Ϊ��null�ͻ��з�
		asciiptr[kPtr_size] = 0;
		RecordWritewLock writew_lock(index_.fd, 0, SEEK_SET, 0);
		if (fstat(index_.fd, &statbuff) < 0) {
			printf("db_open: fstat error\n");
			return false;
		}
		if (!statbuff.st_size) {
			/*
			 * ���Ǳ��빹��һ��ֵΪ0��kHashtablesize + 1������
			 * + 1��ʾhash��֮ǰ�Ŀ����б�ָ��
			 */
			sprintf(asciiptr, "%*d", kPtr_size, 0);
			hash[0] = 0;
			for (int i = 0; i < kHash_table_size + 1; i++)
				strcat(hash, asciiptr);
			strcat(hash, "\n");
			int size = strlen(hash);
			if (write(index_.fd, hash, size) != size) {
				printf("db_open: index file init write error\n");
				return false;
			}
		}
	}
	return true;
}

void DB::_db_bind_function() {
	store_function_map[DB_INSERT] = std::bind(&DB::_db_store_insert, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	store_function_map[DB_REPLACE] = std::bind(&DB::_db_store_replace, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	store_function_map[DB_STORE] = std::bind(&DB::_db_store_ins_or_rep, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
}

bool DB::_db_allocate() {
	if (!(index_.buffer = new char[kIndex_max + 2])) {
		printf("_db_allocate: malloc error for index buffer\n");
		goto allocate_fail;
	}
	if (!(data_.buffer = new char[kData_max + 2])) {
		printf("_db_allocate: malloc error for data buffer\n");
		goto allocate_fail;
	}
	return true;
allocate_fail:
	_db_free();
	return false;
}

/*
 * �ͷ���Դ
 */
void DB::_db_free() {
	if (index_.fd >= 0)
		close(index_.fd);
	if (data_.fd >= 0)
		close(data_.fd);
	if (index_.buffer)
		delete index_.buffer;
	if (data_.buffer)
		delete data_.buffer;
}

void DB::db_close() {
	_db_free();
}

string DB::db_fetch(const string &key) {
	off_t start_offset = _db_hash(key) * kPtr_size + kHash_offset;
	string value;
	//�Ӹ�������ֻ����һ���ֽ�
	RecordReadwLock readw_lock(index_.fd, start_offset, SEEK_SET, 1);
	if (_db_find(key, start_offset)) 
		//���ҳɹ�
		value = _db_read_data();
	return value;
}

/*
 * ����key��hashֵ
 */
DBHASH DB::_db_hash(const string &key) {
	DBHASH hash_value = 0;
	for (int i = 0; i < key.length(); ++i)
		//�ַ���hash
		hash_value = hash_value * kHash_multipy_factor + key[i];
	hash_value %= kHash_table_size;
	return hash_value;
}

/*
 * �����Ƿ�������key
 * �õ���hash��洢�Ľṹ��offsetΪ��Ӧ��hash�������ʼƫ������Ҳ���ǲ��ҵ����
 * ���ô˺���ǰ��Ҫ��������
 * �ɹ�����true��ʧ�ܷ���false
 * ���ҳɹ�����صĽ���洢��index_��data_��
 */
bool DB::_db_find(const string& key, off_t offset) {
	pre_offset_ = offset;
	offset = _db_read_ptr(offset);
	while (offset) {
		off_t next_offset = _db_read_idx(offset);
		if (!strcmp(index_.buffer, key.c_str())) 
			//�ָ����ĵ�һ��Ԫ����key
			return true;
		pre_offset_ = offset;                  //��¼���һ��read_idx��ǰһ���ڵ�
		offset = next_offset;
	}
	return false;
}

/*
 * ��.idx�ļ��е�offsetƫ�������ȡһ��kPtr_size��һ��pointer��������һ���ڵ��ƫ������
 * ����0���Ƕ�ȡʧ�ܻ�����β�ڵ��ˣ��ɹ��򷵻ض�Ӧ��ƫ����
 */
off_t DB::_db_read_ptr(off_t offset) {
	char asciiptr[kPtr_size + 1];
	if (-1 == lseek(index_.fd, offset, SEEK_SET)) {
		printf("_db_read_ptr: lseek error to ptr field\n");
		return 0;
	}
	if (read(index_.fd, asciiptr, kPtr_size) != kPtr_size) {
		printf("_db_read_ptr: read error of ptr field\n");
		return 0;
	}
	asciiptr[kPtr_size] = 0;
	return atol(asciiptr);
}

/*
 * ��idx�ļ���offset��Ľڵ���Ϣ����Handle�Ľṹ��
 * ������һ��index��idx�ļ����ƫ������ʧ�ܷ���0
 */
off_t DB::_db_read_idx(off_t offset) {
	/*
	 * ��λidx�ļ����Ҽ�¼ƫ����
	 * 0 == offset��ʾ�ӵ�ǰƫ������ȡ
	 */
	if (-1 == (index_.offset = lseek(index_.fd, offset, !offset ? SEEK_CUR : SEEK_SET))) {
		printf("_db_read_idx: leek error\n");
		return 0;
	}
	//�ֳ������ֶ�ȡ����һ������ָ����һ���ڵ��ptr���ڶ���������index��¼�ĳ���
	char asciiptr[kPtr_size + 1], ptr_length[kIndex_length_size + 1];
	struct iovec iov[2];
	iov[0].iov_base = asciiptr;
	iov[0].iov_len = kPtr_size;
	iov[1].iov_base = ptr_length;
	iov[1].iov_len = kIndex_length_size;
	size_t read_length;
	if ((read_length = readv(index_.fd, iov, 2)) != kPtr_size + kIndex_length_size) {
		/*
		 * SEEK_CUR��offset==0ʱ���ܻ�����EOF
		 */
		if (!read_length && !offset)
			return 0;
		printf("_db_read_idx: readv error of index record\n");
		return 0;
	}
	//��ֹ��
	asciiptr[kPtr_size] = 0;
	ptr_length[kIndex_length_size] = 0;
	next_offset_ = atol(asciiptr); //��¼���һ��read_idx����һ���ڵ�
	index_.length = atoi(ptr_length);
	//���������ж�
	if ((index_.length < kIndex_min || index_.length > kIndex_max)) {
		printf("_db_read_idx: index length =%d, index length not in range\n", index_.length);
		return 0;
	}
	//��ȡindex��¼
	if (read(index_.fd, index_.buffer, index_.length) != index_.length) {
		printf("_db_read_idx: read error of index record\n");
		return 0;
	}
	//�����Լ��
	if (index_.buffer[index_.length - 1] != kNew_line) {
		printf("_db_read_idx: missing newline\n");
		return 0;
	}
	index_.buffer[index_.length - 1] = 0;  //���з��滻Ϊnull
	char *ptr1, *ptr2;
	ptr2 = index_.buffer + index_.length - 2;
	while (ptr2 >= index_.buffer && kSeparate != *ptr2)
		ptr2--;
	if (ptr2 < index_.buffer){
		printf("_db_read_idx: missing second separator\n");
		return 0;
	}
	*ptr2++ = 0;    //�滻�ָ��Ϊnull
	ptr1 = ptr2 - 2;
	while (ptr1 >= index_.buffer && kSeparate != *ptr1)
		ptr1--;
	if (ptr1 < index_.buffer){
		printf("_db_read_idx: missing first separator\n");
		return 0;
	}
	*ptr1++ = 0;
	/*
	 * �ָ����ֿ����������ݷֱ���key��data��ƫ������data�ĳ���
	 *�����data��ƫ�����ͳ��ȶ���data_��
	 */
	if ((data_.offset = atol(ptr1)) < 0) {
		printf("_db_read_idx: starting offset < 0\n");
		return 0;
	}
	if ((data_.length = atol(ptr2)) <= 0) {
		printf("_db_read_idx: invalid length\n");
		return 0;
	}
	return next_offset_;
}

/*
 * ��data����data_.buffer�ﲢ����
 * ʧ�ܷ���""�ַ���
 */
char *DB::_db_read_data() {
	if (-1 == lseek(data_.fd, data_.offset, SEEK_SET)) {
		printf("_db_read_dat: lseek error\n");
		return "";
	}
	if (read(data_.fd, data_.buffer, data_.length) != data_.length) {
		printf("_db_read_dat: read error\n");
		return "";
	}
	if (data_.buffer[data_.length - 1] != kNew_line) {
		//�����Լ��
		printf("_db_read_dat: missing newline\n");
		return "";
	}
	data_.buffer[data_.length - 1] = 0; //��null�滻���з�
	return data_.buffer;
}

bool DB::db_delete(const string &key) {
	off_t start_offset = _db_hash(key) * kPtr_size + kHash_offset;
	//��ΪҪɾ�����ԼӸ�д����ͬ��ֻ����һ���ֽ�
	RecordReadwLock writew_lock(index_.fd, start_offset, SEEK_SET, 1);
	if (_db_find(key, start_offset)) 
		//�������key
		return _db_do_delete();
	return false;
}

/*
 * ������ɾ������
 * �ɹ�����true��ʧ�ܷ���false
 */
bool DB::_db_do_delete() {
	//�����databuffer
	memset(data_.buffer, kSpace, data_.length);
	data_.buffer[data_.length - 1] = 0;
	//�����indexbuffer
	char *ptr = index_.buffer;
	while (*ptr)
		*ptr++ = kSpace;
	//��ס��������
	RecordWritewLock writew_lock(index_.fd, kFree_offset, SEEK_SET, 1);
	//���յ�databufferд��
	if (!_db_write_data(data_.buffer, data_.offset, SEEK_SET)) {
		printf("_db_do_delete: db_write_data error\n");
		return false;
	}
	off_t save_ptr = next_offset_;     //����һ��ԭ���ڵ����һ���ڵ��ƫ��������Ϊ����ĵ��û��޸����ֵ
	/*
	 * ���ýڵ�ŵ�����������
	 * �������Ϊ���ýڵ��ptr����ָ���������ĵ�һ���ڵ�
	 * �ٽ���������ͷ��ptr����Ϊ�ýڵ��ƫ����
	 */
	off_t free_ptr = _db_read_ptr(kFree_offset);
	if (!_db_write_idx(index_.buffer, index_.offset, SEEK_SET, free_ptr)) {
		printf("_db_do_delete: db write idx error\n");
		return false;
	}
	if (!_db_write_ptr(kFree_offset, index_.offset)) {
		printf("_db_do_delete: db write ptr error\n");
		return false;
	}
	/*
	 * ��ԭ���ýڵ���hash���е�ǰһ���ڵ��ptrָ��ԭ���ýڵ�ĺ�һ���ڵ�
	 * Ҳ���ǰ�����hash����ɾ��
	 */
	if (!_db_write_ptr(pre_offset_, save_ptr)) {
		printf("_db_do_delete: db write ptr error\n");
		return false;
	}
	return true;
}

/*
 * д��һ��data��¼��ƫ����Ϊoffset���������whence
 * �ɹ�����true��ʧ�ܷ���false
 * �˰汾�������汾
 */
bool DB::_db_write_data(const char* data, off_t offset, int whence) {
	if (-1 == (data_.offset = lseek(data_.fd, offset, whence))) {
		printf("_db_write_data: lseek error\n");
		return false;
	}
	struct iovec iov[2];
	char newline = kNew_line;
	//��dataд���.dat�ļ���ÿ��data��¼�����û��з�������
	data_.length = strlen(data) + 1;
	iov[0].iov_base = (char *)data;
	iov[0].iov_len = data_.length - 1;
	iov[1].iov_base = &newline;
	iov[1].iov_len = 1;
	if (writev(data_.fd, iov, 2) != data_.length) {
		printf("_db_write_data: writev error of data record\n");
		return false;
	}
	return true;
}

/*
 * ����ķ����ļ�����
 * ���и��������Ҫ�����ĸ�����
 */
bool DB::_db_lock_and_write_data(const char* data, off_t offset, int whence) {
	//��ס����data�ļ�
	RecordWritewLock writew_lock(data_.fd, 0, SEEK_SET, 0);
	return _db_write_data(data, offset, whence);
}

/*
 * ��offset���λ��дһ��index��¼
 * key�Ǵ��keyֵ��offsetΪд���λ�õ�ƫ������whence��㣬next_offsetΪ�ýڵ����һ��ƫ����
 * �ɹ�����true��ʧ�ܷ���false
 * �˰汾Ϊ�����汾
 */
bool DB::_db_write_idx(const char* key, off_t offset, int whence, off_t next_offset) {
	struct iovec iov[2];
	char prefix[kPtr_size + kIndex_length_size + 1];
	if (!_db_pre_write_idx(key, next_offset, iov, prefix)) {
		printf("_db_writeidx: pre write idx error\n");
		return false;
	}
	if (!_db_do_write_idx(offset, whence, iov)) {
		printf("_db_writeidx: do write idx error\n");
		return false;
	}
	return true;
}

/*
 * _db_write_idx�ļ�����
 */
bool DB::_db_lock_and_write_idx(const char* key, off_t offset, int whence, off_t next_offset) {
	struct iovec iov[2];
	char prefix[kPtr_size + kIndex_length_size + 1];
	if (!_db_pre_write_idx(key, next_offset, iov, prefix)) {
		printf("_db_writeidx: pre write idx error\n");
		return false;
	}
	//ֻ��ס��������ݣ�������סĳ��hash������֮ǰ���м���
	RecordWritewLock writew_lock(index_.fd, ((kHash_table_size + 1) * kPtr_size) + 1, SEEK_SET, 0);
	if (!_db_do_write_idx(offset, whence, iov)) {
		printf("_db_writeidx: do write idx error\n");
		return false;
	}
	return true;
}

/*
 * ����дindex��¼֮ǰ��Ԥ����
 * ��Ҫ�����������ж��Լ�����iovec�ṹ����дindex��¼
 * �����ο�db_write_idx������������������ڽ������
 * �ɹ�����true��ʧ�ܷ���false��iov��ɢ��д�Ľṹ
 */
bool DB::_db_pre_write_idx(const char* key, off_t next_offset, struct iovec *iov, char *prefix) {
	next_offset_ = next_offset;     //��¼һ�����һ��write_idx��¼����һ���ڵ�
	if (next_offset < 0 || next_offset > kPtr_max) {
		printf("_db_writeidx: invalid next_offset: %d", next_offset);
		return false;
	}
	//�ṹ��key:data��ƫ����:data�ĳ��� \n��ÿ����¼�ķָ���
	sprintf(index_.buffer, "%s%c%lld%c%d\n", key, kSeparate, (long long)data_.offset, kSeparate, data_.length);
	index_.length = strlen(index_.buffer);
	if (index_.length < kIndex_min || index_.length > kIndex_max) {
		printf("_db_writeidx: invalid length\n");
		return false;
	}
	//index��¼��ǰ׺���ṹ��next_offset+index_length
	sprintf(prefix, "%*lld%*d", kPtr_size, (long long)next_offset, kIndex_length_size, index_.length);
	iov[0].iov_base = prefix;
	iov[0].iov_len = kPtr_size + kIndex_length_size;
	iov[1].iov_base = index_.buffer;
	iov[1].iov_len = index_.length;
	return true;
}

/*
 * ����д��index�ļ��ĺ����������ο�db_write_idx��_db_pre_write_idx
 * �ɹ�����true��ʧ�ܷ���false
 */
bool DB::_db_do_write_idx(off_t offset, int whence, struct iovec *iov) {
	//��¼һ�µ�ǰindex��¼��ƫ����
	if (-1 == (index_.offset = lseek(index_.fd, offset, whence))) {
		printf("_db_writeidx: lseek error\n");
		return false;
	}
	if (writev(index_.fd, iov, 2) != kPtr_size + kIndex_length_size + index_.length) {
		printf("_db_writeidx: writev error of index record\n");
		return false;
	}
	return true;
}

/*
 * �ڵ�ǰλ��д��ptr
 * �ɹ�����true��ʧ�ܷ���false
 */
bool DB::_db_write_ptr(off_t offset, off_t ptr) {
	char asciiptr[kPtr_size + 1];
	if (ptr < 0 || ptr > kPtr_max) {
		printf("_db_writeptr: invalid ptr: %d\n", ptr);
		return false;
	}
	sprintf(asciiptr, "%*lld", kPtr_size, (long long)ptr);

	if (-1 == lseek(index_.fd, offset, SEEK_SET)) {
		printf("_db_write_ptr: lseek error to ptr field\n");
		return false;
	}
	if (write(index_.fd, asciiptr, kPtr_size) != kPtr_size) {
		printf("_db_write_ptr: write error of ptr field\n");
		return false;
	}
	return true;
}

int DB::db_store(const string &key, const string &data, int flag) {
	//����־�Ϸ���
	if (flag <= STORE_MIN_FLAG || flag >= STORE_MAX_FLAG) {
		printf("_db_store: flag is invalid\n");
		return -1;
	}
	//������ݳ���
	int data_length = data.length() + 1;    //�ǵ��������з�
	if (data_length < kData_min || data_length > kData_max) {
		printf("db_store: invalid data length\n");
		return -1;
	}
	//�ȶ����key��hash���ϸ�д��
	off_t start_offset = _db_hash(key) * kPtr_size + kHash_offset;
	RecordWritewLock writew_lock(index_.fd, start_offset, SEEK_SET, 1);
	bool can_find = _db_find(key, start_offset);
	//��ͬ��flag���ò�ͬ�ĺ���
	return store_function_map[flag](key, data, can_find, start_offset);
}

/*
 * insert����������ֻ��db_store���˸��Ƿ���ڸ�key�ı��(can_find)
 * ����ֵ��db_storeһ��
 */
int DB::_db_store_insert(const string &key, const string &data, bool can_find, off_t start_offset) {
	if (can_find) {
		printf("_db_store_insert: key is exist in db\n");
		return 1;
	}
	int key_length = key.length();
	int data_length = data.length() + 1;    //�ǵ������з�
	off_t ptr = _db_read_ptr(start_offset);    //��¼��ǰhash���ĵ�һ���ڵ��ƫ����
	//�����Ƿ��к��ʵĿ��нڵ�
	if (!_db_find_and_delete_free(key_length, data_length)) {
		/*
		 * û���ҵ���Ѽ�¼׷�ӵ�.idx�ļ���.dat�ļ���β
		 * ���������¼����ŵ����hash����ͷ
		 * ע�⣬�������Ҫ��������
		 */
		if (!_db_lock_and_write_data(data.c_str(), 0, SEEK_END)) {
			printf("_db_store_insert: db lock and write data error\n");
			return -1;
		}
		if (!_db_lock_and_write_idx(key.c_str(), 0, SEEK_END, ptr)) {
			printf("_db_store_insert: db lock and write idx error\n");
			return -1;
		}
	}
	else {
		/*
		 * �ҵ���Ѹü�¼д������ڵ��λ��
		 * ����ڵ��Ѿ������ڿ�����������hash��Ҳ��ס�ˣ�����ֻ��Ҫ���ò�������
		 */
		if (!_db_write_data(data.c_str(), data_.offset, SEEK_SET)) {
			printf("_db_store_insert: db write data error\n");
			return -1;
		}
		if (!_db_write_idx(key.c_str(), index_.offset, SEEK_SET, ptr)) {
			printf("_db_store_insert: db write idx error\n");
			return -1;
		}
	}
	if (!_db_write_ptr(start_offset, index_.offset)) {
		printf("_db_store_insert: db write ptr error\n");
		return -1;
	}
	return 0;
}

/*
 * �����ͷ���ֵ�ο�_db_store_insert
 * replace����
 */
int DB::_db_store_replace(const string &key, const string &data, bool can_find, off_t start_offset) {
	if (!can_find) {
		printf("_db_store_replace: db can not find key\n");
		return -1;
	}
	//���data�ĳ����Ƿ����
	int data_length = data.length() + 1;      //�ǵ������з�
	if (data_length != data_.length) {
		/*
		 * ���Ȳ�һ��
		 * ��ɾ��������ݣ�Ȼ���ٵ���insert����
		 */
		if (!_db_do_delete()) {
			printf("_db_store_replace: db do delete error\n");
			return -1;
		}
		return _db_store_insert(key, data, false, start_offset);
	}
	else {
		/*
		 * ����һ��
		 * ֱ����������ڵ���д����
		 */
		if (!_db_write_data(data.c_str(), data_.offset, SEEK_SET)) {
			printf("_db_store_replace: db write data error\n");
			return -1;
		}
		return 0;
	}
}

/*
 * �����ο�db_store_insert
 * insert����replace����
 */
int DB::_db_store_ins_or_rep(const string &key, const string &data, bool can_find, off_t start_offset) {
	//ûʲô��˵�ģ�����key�͵���replace�������ڵ���insert
	if (can_find)
		return _db_store_replace(key, data, can_find, start_offset);
	else
		return _db_store_insert(key, data, can_find, start_offset);
}

/*
 * �����Ƿ��к��ʵĿ��нڵ�
 * �����������ӿ���������ɾ��
 * �ҽ���Ӧ����Ϣд��index_��data_
 * �ɹ�����true��ʧ�ܷ���false
 */
bool DB::_db_find_and_delete_free(int key_length, int data_length) {
	off_t offset, next_offset;
	//���ϸ�д��
	RecordWritewLock writew_lock(index_.fd, kFree_offset, SEEK_SET, 1);
	pre_offset_ = kFree_offset;
	offset = _db_read_ptr(kFree_offset);
	while (offset) {
		next_offset = _db_read_idx(offset);
		if (strlen(index_.buffer) == key_length && data_.length == data_length)
			//�ҵ��˺��ʵĿ��нڵ�
			break;
		pre_offset_ = offset;    //��¼ǰһ���ڵ�
		offset = next_offset;
	}
	if (!offset)
		return false;
	//�ҵ��˾Ͱ�����ڵ�ӿ���������ȥ��
	return _db_write_ptr(pre_offset_, next_offset_);
}

}
