#include "include/record_lock.h"

#include <cstdio>
#include <stdlib.h>

RecordLock::RecordLock(int fd, off_t offset, int whence, off_t len)
	:	fd_(fd),
		offset_(offset),
		whence_(whence),
		len_(len),
		lock_result_(0),
		unlock_result_(0)
{}

RecordLock::~RecordLock() {
	if ((unlock_result_ = un_lock()) < 0)
		fail_unlock_process();
}

void RecordLock::fail_lock_process() {
	printf("%s fail lock\n", lockname_);
	abort();
	//�����䴦��
}

void RecordLock::fail_unlock_process() {
	printf("%s fail unlock\n", lockname_);
	abort();
	//�����䴦��
}

int RecordLock::lock_result() {
	return lock_result_;
}

int RecordLock::unlock_result() {
	return unlock_result_;
}

/*
 * �������߽���һ������
 */
int RecordLock::lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
	struct flock lock;
	lock.l_type = type;                 //��������F_RDLCK,R_WRLCK,F_UNLCK
	lock.l_start = offset;              //�����whence��ƫ����
	lock.l_whence = whence;             //�������SEEK_SET,SEEK_CUR,SEEK_END��ͷ����ǰ��β
	lock.l_len = len;                   //�ֽڳ��� 0��ζ�����ķ�Χ������չ�������ܵ�ƫ����������׷�Ӷ������ݶ������ķ�Χ��
	return fcntl(fd, cmd, &lock);
}

int RecordLock::lock(){}

int RecordLock::un_lock() {
	return lock_reg(fd_, F_SETLK, F_UNLCK, offset_, whence_, len_);
}

RecordReadLock::RecordReadLock(int fd, off_t offset, int whence, off_t len) 
	:	RecordLock(fd, offset, whence, len)
{
	lockname_ = "RecordReadLock";
	if ((lock_result_ = lock()) < 0)
		fail_lock_process();
}

int RecordReadLock::lock() {
	return lock_reg(fd_, F_SETLK, F_RDLCK, offset_, whence_, len_);
}

RecordReadwLock::RecordReadwLock(int fd, off_t offset, int whence, off_t len) 
	:	RecordLock(fd, offset, whence, len)
{
	lockname_ = "RecordReadwLock";
	if ((lock_result_ = lock()) < 0)
		fail_lock_process();
}

int RecordReadwLock::lock() {
	return lock_reg(fd_, F_SETLKW, F_RDLCK, offset_, whence_, len_);
}

RecordWriteLock::RecordWriteLock(int fd, off_t offset, int whence, off_t len)
	: RecordLock(fd, offset, whence, len)
{
	lockname_ = "RecordWriteLock";
	if ((lock_result_ = lock()) < 0)
		fail_lock_process();
}

int RecordWriteLock::lock() {
	return lock_reg(fd_, F_SETLK, F_WRLCK, offset_, whence_, len_);
}

RecordWritewLock::RecordWritewLock(int fd, off_t offset, int whence, off_t len)
	: RecordLock(fd, offset, whence, len)
{
	lockname_ = "RecordWritewLock";
	if ((lock_result_ = lock()) < 0)
		fail_lock_process();
}

int RecordWritewLock::lock() {
	return lock_reg(fd_, F_SETLKW, F_WRLCK, offset_, whence_, len_);
}

