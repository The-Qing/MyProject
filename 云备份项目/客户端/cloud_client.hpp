#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>//split头文件 
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
	//从文件中读取所有内容
	static bool Read(const std::string &name, std::string *body) {
		//注意以二进制方式打开文件
		std::ifstream fs(name, std::ios::binary);//输入文件流
		if (fs.is_open() == false) {
			std::cout << " open file " << name << " failed\n";
			return false;
		}
		int64_t fsize = boost::filesystem::file_size(name);
		body->resize(fsize);//给body申请空间
		fs.read(&(*body)[0], fsize);
		//fs.good()判断上一操作是否成功
		if (fs.good() == false) {
			std::cout << " file " << name << " read data failed\n";
			return false;
		}
		fs.close();
		return true;
	}

	//向文件中写入数据
	static bool Write(const std::string &name, const std::string &body) {
		//输出流,ofstream默认打开文件时清空原有内容
		//覆盖写入
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
	//插入更新数据
	bool Insert(const std::string &key, const std::string &val) {
		_backup_list[key] = val;
		return true;
	}
	//通过文件名获取原有etag信息
	bool GetEtag(const std::string &key, std::string *val) {
		auto it = _backup_list.find(key);
		if (it == _backup_list.end()) {
			return false;
		}
		*val = it->second;
		return false;
	}
	//持久化存储
	bool Storage() {
		//将_file_list中的数据进行持久化存储
		//数据对象进行持久化存储需要先进行序列化
		//按照指定格式
		std::stringstream tmp;//实例化出的一个string流对象
		auto it = _backup_list.begin();
		//遍历完成后就将所有的信息加入到这个tmp string流中了
		for (; it != _backup_list.end(); it++) {
			tmp << it->first << " " << it->second << "\r\n";
		}
		//将tmp中数据写入_back_file中
		//tmp.str()是string流中的string对象
		FileUtil::Write(_store_file, tmp.str());
		return true;
	}
	//初始化加载原有数据
	bool InitLoad() {
		//从数据的持久化存储文件中加载数据
		//1.将这个备份文件的数据读取出来
		std::string body;
		if (FileUtil::Read(_store_file, &body) == false) {
			return false;
		}
		//2.进行字符串处理，按照\r\n对body进行分割,分割完成放入list
		//boost::split(vector,src,sep,flag)
		std::vector<std::string> list;
		boost::split(list, body, boost::is_any_of("\r\n"), boost::token_compress_off);
		//3.每一行按照空格进行分割，前面是key，后面是val
		for (auto i : list) {
			size_t pos = i.find(" ");
			if (pos == std::string::npos) {
				continue;
			}
			std::string key = i.substr(0, pos);
			std::string val = i.substr(pos + 1);
			//4.将key/val添加到_file_list中
			Insert(key, val);
			//更新完毕后进行持久化存储
			Storage();
		}
		return true;
	}
private:
	std::string _store_file;//持久化存储文件名称
	std::unordered_map<std::string, std::string> _backup_list;//备份的文件信息
};

//#define STORE_FILE "./list.backup"
//#define LISTEN_DIR "./backup/"
class CloudClient {
public:
	CloudClient(const std::string &filename , const std::string &store_file,const std::string &srv_ip,const uint16_t srv_port)
						:_listen_dir(filename),data_manage(store_file),_srv_ip(srv_ip),_srv_port(srv_port){}
	//完成整体的文件备份流程
	bool Start() {
		//先初始化加载初始信息
		data_manage.InitLoad();
		while (1) {
			std::vector<std::string> list;
			GetBackupFileList(&list);//获取所有需要备份的文件名称
			for (int i = 0; i < list.size(); i++) {
				std::string name = list[i];//文件名
				std::string pathname = _listen_dir + name;//文件的路径名

				std::cout << pathname << " is need to backup\n";
				//读取文件数据，作为请求正文
				std::string body;
				FileUtil::Read(pathname, &body);
				//从实例化Client对象准备发起HTTP上传文件请求
				httplib::Client client(_srv_ip, _srv_port);
				std::string req_path = "/" + name;
				auto rsp = client.Put(req_path.c_str(), body, "application/octet-stream");//第三个参数表示正文的数据类型为二进制数据流
				if ((char)rsp == NULL || ((char)rsp != NULL && rsp->status != 200)) {
					continue; //文件上传备份失败
					std::cout << pathname << " backup failed\n";
				}
				std::string etag;
				GetEtag(pathname, &etag);
				data_manage.Insert(name, etag);//备份成功则插入，更新信息

				std::cout << pathname << " backup success\n";
			}
			Sleep(1000);
		}
		return true;
	}
	//获取需要备份的文件列表，不带路径
	bool GetBackupFileList(std::vector<std::string> *list) {
		//目录不存在则创建
		if (boost::filesystem::exists(_listen_dir) == false){
			boost::filesystem::create_directory(_listen_dir);
		}
		//1.进行目录监控，获取指定目录下所有文件名
		boost::filesystem::directory_iterator begin(_listen_dir);
		boost::filesystem::directory_iterator end;

		for (; begin != end; begin++){
			//判断是否为一个目录
			if (boost::filesystem::is_directory(begin->status())){
				//目录是不需要进行备份
				//不备份多层级目录
				continue;
			}
			std::string pathname = begin->path().string();
			std::string name = begin->path().filename().string();
			std::string cur_etag;
			std::string old_etag;

		    //2.逐个计算当前文件的etag
			GetEtag(pathname, &cur_etag);
			//3.获取已经备份的etag信息
			data_manage.GetEtag(name, &old_etag);
			//4.与data_manage中保存的原有etag进行比对
			//	1.没有找到原有etag信息表示为新文件需要备份
			//	2.找到原有etag，但是当前etag与原有etag不相等，需要备份
			//	3.找到原有etag，并且与当前etag信息相等，不需要备份
			if (cur_etag != old_etag) {
				list->push_back(name);//当前etag与原有etag不同则备份
			}
		}
		return true;
	}
	//计算文件的etag信息
	bool GetEtag(const std::string &pathname, std::string *etag) {
		//etag:文件大小，文件最后一次修改信息等等
		int64_t fsize = boost::filesystem::file_size(pathname);
		time_t mtime = boost::filesystem::last_write_time(pathname);
		*etag = std::to_string(fsize) + "-" + std::to_string(mtime);
		return true;
	}
private:
	std::string _srv_ip;//服务端ip
	uint16_t _srv_port;//服务端端口
	std::string _listen_dir;//监控的目录名称
	DataManage data_manage;
};

