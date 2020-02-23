#pragma once

#include <fcntl.h>

/*
 * RAII��װ�ļ�¼��
 * ����ʱ���ö�Ӧ��lock�����������뿪��������Զ�����
 * ע�⣬��Ӧ���Լ��ֶ�����
 */
class RecordLock {
public:
	/*
	 * ��һ��������fd����Ӧ�������ļ�������
	 * �ڶ���������offset�����ƫ����
	 * �����������ǲ��������SEEK_SET,SEEK_CUR,SEEK_END
	 * ���ĸ������ǳ���
	 * ���캯�������lock��������Ҫ��дlock����
	 */
	explicit RecordLock(int, off_t, int, off_t);
	/*
	 * ������������un_lock����
	 */
	virtual ~RecordLock();
	int lock_result();
	int unlock_result();

protected:
	char *lockname_;        //��Ӧ����������
	int fd_;                //��Ӧ�������ļ�fd	
	off_t offset_;          //ƫ����
	int whence_;            //���
	off_t len_;             //����
	int lock_result_;        //����lock�����ķ��ؽ��
	int unlock_result_;      //����unlock�����ķ��ؽ��
	/*
	 * ��ͬ����Ӧ����������
	 */
	virtual int lock();
	/*
	 * ��������������ͬ���ǽ�����һ���Ĺʸú�������Ҫ��д
	 */
	int un_lock();
	/*
	 * ����ʧ�ܵĴ�����
	 */
	void fail_lock_process();
	int lock_reg(int, int, int, off_t, int, off_t);

private:
	void fail_unlock_process();
};

/*
 * ����������
 */
class RecordReadLock :public RecordLock {
public:
	explicit RecordReadLock(int, off_t, int, off_t);
protected:
	virtual int lock();
};

/*
 * ��������
 */
class RecordReadwLock :public RecordLock {
public:
	explicit RecordReadwLock(int, off_t, int, off_t);
protected:
	virtual int lock();
};

/*
 * ������д��
 */
class RecordWriteLock :public RecordLock {
public:
	explicit RecordWriteLock(int, off_t, int, off_t);
protected:
	virtual int lock();
};

/*
 * ����д��
 */
class RecordWritewLock :public RecordLock {
public:
	explicit RecordWritewLock(int, off_t, int, off_t);
protected:
	virtual int lock();
};
