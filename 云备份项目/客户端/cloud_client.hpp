#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>//splitͷ�ļ� 
#include<boost/filesystem.hpp>
#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<string>
#include<unordered_map>
#include"httplib.h"

class FileUtil {
public:
	//���ļ��ж�ȡ��������
	static bool Read(const std::string &name, std::string *body) {
		//ע���Զ����Ʒ�ʽ���ļ�
		std::ifstream fs(name, std::ios::binary);//�����ļ���
		if (fs.is_open() == false) {
			std::cout << " open file " << name << " failed\n";
			return false;
		}
		int64_t fsize = boost::filesystem::file_size(name);
		body->resize(fsize);//��body����ռ�
		fs.read(&(*body)[0], fsize);
		//fs.good()�ж���һ�����Ƿ�ɹ�
		if (fs.good() == false) {
			std::cout << " file " << name << " read data failed\n";
			return false;
		}
		fs.close();
		return true;
	}

	//���ļ���д������
	static bool Write(const std::string &name, const std::string &body) {
		//�����,ofstreamĬ�ϴ��ļ�ʱ���ԭ������
		//����д��
		std::ofstream ofs(name, std::ios::binary);
		if (ofs.is_open() == false) {
			std::cout << " open file " << name << " failed\n";
			return false;
		}
		ofs.write(&body[0], body.size());
		if (ofs.good() == false) {
			std::cout << " file " << name << " write data failed!\n";
			return false;
		}
		ofs.close();
		return true;
	}
};

class DataManage {
public:
	DataManage(const std::string &filename):_store_file(filename){}
	//�����������
	bool Insert(const std::string &key, const std::string &val) {
		_backup_list[key] = val;
		return true;
	}
	//ͨ���ļ�����ȡԭ��etag��Ϣ
	bool GetEtag(const std::string &key, std::string *val) {
		auto it = _backup_list.find(key);
		if (it == _backup_list.end()) {
			return false;
		}
		*val = it->second;
		return false;
	}
	//�־û��洢
	bool Storage() {
		//��_file_list�е����ݽ��г־û��洢
		//���ݶ�����г־û��洢��Ҫ�Ƚ������л�
		//����ָ����ʽ
		std::stringstream tmp;//ʵ��������һ��string������
		auto it = _backup_list.begin();
		//������ɺ�ͽ����е���Ϣ���뵽���tmp string������
		for (; it != _backup_list.end(); it++) {
			tmp << it->first << " " << it->second << "\r\n";
		}
		//��tmp������д��_back_file��
		//tmp.str()��string���е�string����
		FileUtil::Write(_store_file, tmp.str());
		return true;
	}
	//��ʼ������ԭ������
	bool InitLoad() {
		//�����ݵĳ־û��洢�ļ��м�������
		//1.����������ļ������ݶ�ȡ����
		std::string body;
		if (FileUtil::Read(_store_file, &body) == false) {
			return false;
		}
		//2.�����ַ�����������\r\n��body���зָ�,�ָ���ɷ���list
		//boost::split(vector,src,sep,flag)
		std::vector<std::string> list;
		boost::split(list, body, boost::is_any_of("\r\n"), boost::token_compress_off);
		//3.ÿһ�а��տո���зָǰ����key��������val
		for (auto i : list) {
			size_t pos = i.find(" ");
			if (pos == std::string::npos) {
				continue;
			}
			std::string key = i.substr(0, pos);
			std::string val = i.substr(pos + 1);
			//4.��key/val��ӵ�_file_list��
			Insert(key, val);
			//������Ϻ���г־û��洢
			Storage();
		}
		return true;
	}
private:
	std::string _store_file;//�־û��洢�ļ�����
	std::unordered_map<std::string, std::string> _backup_list;//���ݵ��ļ���Ϣ
};

//#define STORE_FILE "./list.backup"
//#define LISTEN_DIR "./backup/"
class CloudClient {
public:
	CloudClient(const std::string &filename , const std::string &store_file,const std::string &srv_ip,const uint16_t srv_port)
						:_listen_dir(filename),data_manage(store_file),_srv_ip(srv_ip),_srv_port(srv_port){}
	//���������ļ���������
	bool Start() {
		//�ȳ�ʼ�����س�ʼ��Ϣ
		data_manage.InitLoad();
		while (1) {
			std::vector<std::string> list;
			GetBackupFileList(&list);//��ȡ������Ҫ���ݵ��ļ�����
			for (int i = 0; i < list.size(); i++) {
				std::string name = list[i];//�ļ���
				std::string pathname = _listen_dir + name;//�ļ���·����

				std::cout << pathname << " is need to backup\n";
				//��ȡ�ļ����ݣ���Ϊ��������
				std::string body;
				FileUtil::Read(pathname, &body);
				//��ʵ����Client����׼������HTTP�ϴ��ļ�����
				httplib::Client client(_srv_ip, _srv_port);
				std::string req_path = "/" + name;
				auto rsp = client.Put(req_path.c_str(), body, "application/octet-stream");//������������ʾ���ĵ���������Ϊ������������
				if ((char)rsp == NULL || ((char)rsp != NULL && rsp->status != 200)) {
					continue; //�ļ��ϴ�����ʧ��
					std::cout << pathname << " backup failed\n";
				}
				std::string etag;
				GetEtag(pathname, &etag);
				data_manage.Insert(name, etag);//���ݳɹ�����룬������Ϣ

				std::cout << pathname << " backup success\n";
			}
			Sleep(1000);
		}
		return true;
	}
	//��ȡ��Ҫ���ݵ��ļ��б�����·��
	bool GetBackupFileList(std::vector<std::string> *list) {
		//Ŀ¼�������򴴽�
		if (boost::filesystem::exists(_listen_dir) == false){
			boost::filesystem::create_directory(_listen_dir);
		}
		//1.����Ŀ¼��أ���ȡָ��Ŀ¼�������ļ���
		boost::filesystem::directory_iterator begin(_listen_dir);
		boost::filesystem::directory_iterator end;

		for (; begin != end; begin++){
			//�ж��Ƿ�Ϊһ��Ŀ¼
			if (boost::filesystem::is_directory(begin->status())){
				//Ŀ¼�ǲ���Ҫ���б���
				//�����ݶ�㼶Ŀ¼
				continue;
			}
			std::string pathname = begin->path().string();
			std::string name = begin->path().filename().string();
			std::string cur_etag;
			std::string old_etag;

		    //2.������㵱ǰ�ļ���etag
			GetEtag(pathname, &cur_etag);
			//3.��ȡ�Ѿ����ݵ�etag��Ϣ
			data_manage.GetEtag(name, &old_etag);
			//4.��data_manage�б����ԭ��etag���бȶ�
			//	1.û���ҵ�ԭ��etag��Ϣ��ʾΪ���ļ���Ҫ����
			//	2.�ҵ�ԭ��etag�����ǵ�ǰetag��ԭ��etag����ȣ���Ҫ����
			//	3.�ҵ�ԭ��etag�������뵱ǰetag��Ϣ��ȣ�����Ҫ����
			if (cur_etag != old_etag) {
				list->push_back(name);//��ǰetag��ԭ��etag��ͬ�򱸷�
			}
		}
		return true;
	}
	//�����ļ���etag��Ϣ
	bool GetEtag(const std::string &pathname, std::string *etag) {
		//etag:�ļ���С���ļ����һ���޸���Ϣ�ȵ�
		int64_t fsize = boost::filesystem::file_size(pathname);
		time_t mtime = boost::filesystem::last_write_time(pathname);
		*etag = std::to_string(fsize) + "-" + std::to_string(mtime);
		return true;
	}
private:
	std::string _srv_ip;//�����ip
	uint16_t _srv_port;//����˶˿�
	std::string _listen_dir;//��ص�Ŀ¼����
	DataManage data_manage;
};

