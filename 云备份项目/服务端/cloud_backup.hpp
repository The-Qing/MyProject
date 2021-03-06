#include<cstdio>
#include<string>
#include<vector>
#include<fstream>
#include<unordered_map>
#include<zlib.h>
#include<pthread.h>
#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>
#include<iostream>
#include"httplib.h"

#define NONHOT_TIME 10 //最后一次访问时间大于10秒
#define INTERVAL_TIME 30 //非热点文件的检测每30秒一次
#define BACKUP_DIR "./backup/" //文件备份路径
#define GZFILE_DIR "./gzfile/" //压缩包存放路径
#define DATA_FILE "./list.backup"//数据管理模块的数据备份文件名

namespace _cloud_sys{
    class FileUtil{
	public:
	//从文件中读取所有内容
	static bool Read(const std::string &name,std::string *body){
		//注意以二进制方式打开文件
		std::ifstream fs(name,std::ios::binary);//输入文件流
		if(fs.is_open() == false){
			std::cout<<"open file"<<name<<"failed\n";
			return false;
		}
		int64_t fsize = boost::filesystem::file_size(name);
		body->resize(fsize);//给body申请空间
		fs.read(&(*body)[0],fsize);
		//fs.good()判断上一操作是否成功
		if(fs.good() == false){
			std::cout<<"file"<<name<<"read data failed\n";
			return false;
		}
		fs.close();
		return true;
	}

	//向文件中写入数据
	static bool Write(const std::string &name,const std::string &body){
		//输出流,ofstream默认打开文件时清空原有内容
		//覆盖写入
		std::ofstream ofs(name,std::ios::binary);
		if(ofs.is_open() == false){
			std::cout<<"open file"<<name<<"failed\n";
			return false;
		}
		ofs.write(&body[0],body.size());
		if(ofs.good() == false){
			std::cout<<"file"<<name<<"write data failed!\n";
			return false;
		}
		ofs.close();
		return true;
	}
    };
    class CompressUtil{
    	public:
	//文件压缩（源文件名，压缩包名）
	static bool Compress(const std::string &src,const std::string &dst){
		std::string body;
		FileUtil::Read(src,&body);
			
		gzFile gf = gzopen(dst.c_str(),"wb");//打开压缩包
		if(gf == NULL){
			std::cout<<"open file "<<dst<<"failed! \n";
			return false;
		}
		int wlen = 0;
		//如果一次没有将全部数据压缩，则从未压缩的部分开始压缩
		while(wlen<body.size()){
		int ret = gzwrite(gf,&body[wlen],body.size() - wlen);
		if(ret == 0){
			std::cout<<"file "<<dst<<"write compress data failed!\n";
			return false;
		}
		wlen += ret;
		}
		gzclose(gf);
		return true;
	}
	
	//文件解压缩（压缩包名称，源文件名称）
	//读多少写多少
	static bool UnCompress(const std::string &src,const std::string &dst){
		std::ofstream ofs(dst,std::ios::binary);
		if(ofs.is_open() == false){
			std::cout<<"open file"<<dst<<"failed!\n";
			return false;
		}
		gzFile gf = gzopen(src.c_str(),"rb");
		if(gf == NULL){
			std::cout<<"open file"<<src<<"failed!\n";
			ofs.close();
			return false;
		}
		int ret;
		char tmp[4096] = {0};
		while((ret = gzread(gf,tmp,4096)) > 0){
			ofs.write(tmp,ret);
		}
		ofs.close();
		gzclose(gf);
		return true;
	}
    };

    class DataManage{
	  public:
		DataManage(const std::string &path):_back_file(path){
			pthread_rwlock_init(&_rwlock,NULL);
		}
		~DataManage(){
			pthread_rwlock_destroy(&_rwlock);
		}
		//判断文件是否存在
		bool Exists(const std::string &name){
			//是否能够从_file_list找到这个文件信息
			//因为_file_list是临界资源所以要加锁访问
			pthread_rwlock_rdlock(&_rwlock);
			auto it = _file_list.find(name);
			if(it == _file_list.end()){
				return false;
			}
			pthread_rwlock_unlock(&_rwlock);
			return true;
		}
		//判断文件是否已经压缩
		bool IsCompress(const std::string &name){
			//管理的数据，源文件名称，压缩包名称
			//文件上传后，源文件名称和压缩包名称一致
			//文件压缩后，将压缩包名称更新为具体的包名
			//因为_file_list是临界资源所以要加锁访问
			pthread_rwlock_rdlock(&_rwlock);
			auto it = _file_list.find(name);
			if(it == _file_list.end()){
				pthread_rwlock_unlock(&_rwlock);
				return false;//表示未找到
			}
			if(it->first == it->second){
				pthread_rwlock_unlock(&_rwlock);
				return false;//名称一样时表示未压缩
			}
			pthread_rwlock_unlock(&_rwlock);
			return true;
		}
		//获取未压缩文件列表
		bool NonCompressList(std::vector<std::string> *list){
			//遍历_file_list将没有压缩的文件名称添加到list中
			pthread_rwlock_rdlock(&_rwlock);
			auto it = _file_list.begin();
			for(;it != _file_list.end();it++){
				if(it->first == it->second){
					list->push_back(it->first);
				}
			}
			pthread_rwlock_unlock(&_rwlock);
			return true; 
		}	
		//插入/更新数据
		bool Insert(const std::string &src,const std::string &dst){
			//因为unordered_map中对=进行了重载所以可以直接使用
			//因为要进行写入操作，所以使用写锁
			pthread_rwlock_wrlock(&_rwlock);			
			_file_list[src] = dst;
			pthread_rwlock_unlock(&_rwlock);
			Storage();//数据更新需重新备份
			return true;
		}
		//获取所有文件名称,向外展示文件列表使用
		bool GetAllName(std::vector<std::string> *list){
			pthread_rwlock_rdlock(&_rwlock);
			auto it = _file_list.begin();
			for(;it != _file_list.end();it++){
				//获取源文件名称
				list->push_back(it->first);
			}
			pthread_rwlock_unlock(&_rwlock);
			return true;
		}
		//根据源文件名称获取压缩包名称
		bool GetGzName(const std::string &src,std::string *dst){
			auto it = _file_list.find(src);
			if(it == _file_list.end()){
				return false;
			}
			*dst = it->second;
			return true;
		}
		//数据改变后进行持久化存储，存储的是管理的文件名数据
		bool Storage(){
			//将_file_list中的数据进行持久化存储
			//数据对象进行持久化存储需要先进行序列化
			//按照指定格式
			std::stringstream tmp;//实例化出的一个string流对象
			//更新数据使用写锁
			pthread_rwlock_wrlock(&_rwlock);
			auto it = _file_list.begin();
			//遍历完成后就将所有的信息加入到这个tmp string流中了
			for(;it != _file_list.end();it++){
				tmp<< it->first << " " << it->second <<"\r\n";
			}
			pthread_rwlock_unlock(&_rwlock);
			//将tmp中数据写入_back_file中
			//tmp.str()是string流中的string对象
			FileUtil::Write(_back_file,tmp.str());
			return true;
		}
		//启动时初始化加载原有数据
		//格式：filename gzfilename\r\nfilename gzfilename\r\n...
		bool InitLoad(){
			//从数据的持久化存储文件中加载数据
			//1.将这个备份文件的数据读取出来
			std::string body;
			if(FileUtil::Read(_back_file,&body) == false){
				return false;
			}
			//2.进行字符串处理，按照\r\n对body进行分割,分割完成放入list
			//boost::split(vector,src,sep,flag)
			std::vector<std::string> list;
			boost::split(list,body,boost::is_any_of("\r\n"),boost::token_compress_off);
			//3.每一行按照空格进行分割，前面是key，后面是val
			for(auto i : list){
				size_t pos = i.find(" ");
				if(pos == std::string::npos){
					continue;
				}
				std::string key = i.substr(0,pos);
				std::string val = i.substr(pos+1);
			//4.将key/val添加到_file_list中
			Insert(key,val);
			}
			return true;
		}
	  private:
		std::string _back_file; //持久化数据存储文件名
		std::unordered_map<std::string,std::string> _file_list;//数据管理容器
		pthread_rwlock_t _rwlock;//读写锁
    };
	_cloud_sys::DataManage data_manage(DATA_FILE);
	
    class NonHotCompress{
	public:
	NonHotCompress(const std::string gz_dir,const std::string bu_dir)
			:_gz_dir(gz_dir),_bu_dir(bu_dir){
	}
	~NonHotCompress(){
	}
	bool Start(){//总体向外提供功能接口，开始压缩模块
		//一个循环持续过程，每隔一段时间就去判断有没有非热点文件，然后压缩操作
		//当前时间减去最后一次访问时间大于n就是非热点文件
		while(1){
			//1.获取所有未压缩的文件列表
			std::vector<std::string> list;
			data_manage.NonCompressList(&list);
			//2.逐个判断这个文件是否为热点文件
			for(int i = 0;i< list.size();i++){
				bool ret = FileIsHot(list[i]);
		   		if(ret == false){
					std::cout<<"non hot file "<<list[i]<<"\n";
					std::string s_filename = list[i];//源文件名
					std::string d_filename = list[i] + ".gz";//压缩包名
					//源文件路径
					std::string src_name = _bu_dir + s_filename;
					//压缩包路径
					std::string dst_name = _gz_dir + d_filename;
					//3.若为非热点文件，则压缩此文件，并删除源文件
      				if(CompressUtil::Compress(src_name,dst_name)==true){
				data_manage.Insert(s_filename,d_filename);//更新数据信息
				unlink(src_name.c_str());//删除源文件
				}
				}
			}
			//4.休眠一段时间
			sleep(INTERVAL_TIME);
		}
		return true;
	}
	private:
	//判断一个文件是否是一个热点文件
	bool FileIsHot(const std::string &name){
		time_t cur_t = time(NULL);//当前时间
		struct stat st; //stat结构体中保存着文件的访问时间等信息
		if(stat(name.c_str(),&st) < 0){
			std::cout<<"get file "<<name<<" stat failed! \n";
			return false;
		}
		//如果为非热点文件
		if((cur_t - st.st_atime) > NONHOT_TIME){
			return false;
		}
		return true;//小于NONHOT_TIME都为热点文件 
	}
	private:
	std::string _bu_dir;//压缩前文件的存储路径
	std::string _gz_dir;//压缩文件存储路径
    };
    class Server{
	public:
	Server(){
	}
	~Server(){
	}
	bool Start(){//启动网络通信模块接口
		//注册路由信息
		//根据不同请求回调相应的函数处理
		_server.Put("/(.*)",Upload);
		_server.Get("/list",List);	
		_server.Get("/download/(.*)",Download);//(.*)表示捕捉任意匹配的字符串
		//开始监听，搭建TCP服务器，等待客户端的连接,进行http数据处理
		_server.listen("0.0.0.0",10000);//监听本机的任意一块网卡ip地址
		return true;
	}
	private:
	//从文件上传处理回调函数
	//文件备份
	static void Upload(const httplib::Request &req,httplib::Response &rsp){
		std::string filename = req.matches[1];//上面捕捉的文件名
		std::string pathname = BACKUP_DIR + filename;//文件路径名，备份到指定路径
		FileUtil::Write(pathname,req.body);//向文件写入数据，文件不存在则创建
		data_manage.Insert(filename,filename);//添加文件信息到数据管理模块
		rsp.status = 200;
		return ;
	}
	//文件列表处理回调函数 
	//获取文件列表 
	//通过data_manage数据管理获取文件列表
	//组织响应的html网页数据
	//填充rsp的正文与状态码和头部信息      
	static void List(const httplib::Request &req,httplib::Response &rsp){
		std::vector<std::string> list;
		data_manage.GetAllName(&list);
		std::stringstream tmp;
		tmp << "<html><body><hr />";
		for(int i = 0;i<list.size();i++){
			//第二个list[i]是一个超链接，点击后向服务器发送href后面的链接请求
			tmp<<"<a href='/download/"<< list[i]<<"'>"<<list[i]<<"</a>";
			tmp<<"<hr />";
		}
		tmp << "<hr /></body></html>";
		
		rsp.set_content(tmp.str().c_str(),tmp.str().size(),"text/html");
		rsp.status = 200;
		return ;
		
	}
	//文件下载处理回调函数
	//从数据模块判断文件是否存在
	//判断文件是否已经压缩，压缩了则先解压缩，然后再读取文件数据
	static void Download(const httplib::Request &req,httplib::Response &rsp){
		std::string filename = req.matches[1];
		if(data_manage.Exists(filename) == false){
			rsp.status = 404;//文件不存在
			return ;
		}	
		std::string pathname = BACKUP_DIR + filename;//源文件的存放路径
		if(data_manage.IsCompress(filename) == true){
			//文件被压缩，先解压文件
			std::string gzfile;
			data_manage.GetGzName(filename,&gzfile);//获取压缩包名
			std::string gzpathname = GZFILE_DIR + gzfile;//组织一个压缩包路径名
			CompressUtil::UnCompress(gzpathname,pathname);//将压缩包解压	
			unlink(gzpathname.c_str());//删除压缩包
			data_manage.Insert(filename,filename);//更新数据信息	
		}
		//从文件中读取数据，响应给客户端
		FileUtil::Read(pathname,&rsp.body);//直接将文件数据读取到rsp的body中
		rsp.set_header("Content-Type","application/octet-stream");//二进制流下载
		rsp.status = 200;
		return ;
	}
	private:
	std::string _file_dir;//文件上传备份路径
	httplib::Server _server;
    };	











}

