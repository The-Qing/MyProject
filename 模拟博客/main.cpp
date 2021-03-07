#include"httplib.h"
#include "db.hpp"

using namespace httplib;

blog_system::TableBlog *table_blog;
blog_system::TableTag *table_tag;

//业务处理
//博客业务处理
void InsertBlog(const Request &req,Response &rsp){
	//从请求中取出正文--正文就是提交的博客信息，以json格式的字符串组织的
	//将json格式的字符串进行反序列化，得到各个博客信息
	Json::Reader reader;
	Json::Value blog;
	Json::FastWriter writer;
	Json::Value errmsg;
	bool ret = reader.parse(req.body,blog);
	if(ret == false){
		printf("InsertBlog parse blog json failed\n");
		rsp.status = 400;
		errmsg["ok"] = false;
		errmsg["reason"] = "parse blog json failed";
		//添加正文到rsp.body中
		rsp.set_content(writer.write(errmsg),"application/json");
		return ;
	}	
	//将得到的博客信息插入到数据库中
	ret = table_blog->Insert(blog);
	if(ret == false){
		printf("InsertBlog insert into database failed!\n");
		rsp.status = 500;
		return ;
	}
	rsp.status = 200;
	return ;
}

void DeleteBlog(const Request &req,Response &rsp){
   // /blog/123 通过正则表达式/blog/(\d+) req.matches[0]=/blog/123 req.matches[0]=123
	int blog_id = std::stoi(req.matches[1]);
	bool ret = table_blog->Delete(blog_id);
	Json::Value errmsg;
	Json::FastWriter writer;
	if(ret == false){
		printf("DeleteBlog delete from database failed!\n");
		rsp.status = 500;
		errmsg["ok"] = false;
		errmsg["reason"] = "delete from database failed!";
		rsp.set_content(writer.write(errmsg),"application/json");
		return ;
	}		
	rsp.status = 200;
	return ;
}

void UpdateBlog(const Request &req,Response &rsp){
	int blog_id = std::stoi(req.matches[1]);
	Json::Value blog;
	Json::Reader reader;
	Json::FastWriter writer;
	Json::Value errmsg;
	bool ret = reader.parse(req.body,blog);
	if(ret == false){
		printf("UpdateBlog parse json failed!\n");
		rsp.status = 400;
		errmsg["ok"] = false;
		errmsg["reason"] = "parse json failed!";
		rsp.set_content(writer.write(errmsg),"application/json");
		return ;
	}
	blog["id"] = blog_id;
	ret = table_blog->Update(blog);
	if(ret == false){
		printf("UpdateBlog update database failed!\n");
		rsp.status = 500;
		return ;
	}
	rsp.status = 200;
	return ;
}

void GetListBlog(const Request &req,Response &rsp){
	//从数据库中取出博客列表数据
	Json::FastWriter writer;
	Json::Value blogs;
	bool ret = table_blog->GetList(&blogs);
	if(ret == false){
		printf("GetListBlog select from database failed!\n");
		rsp.status = 500;
		return ;
	}
	//将数据进行json序列化，添加到rsp正文中
	rsp.set_content(writer.write(blogs),"application/json");
	rsp.status = 200; 
	return;
}

void GetOneBlog(const Request &req,Response &rsp){
	Json::Value blog;
	Json::FastWriter writer;
	int blog_id = std::stoi(req.matches[1]);
	blog["id"] = blog_id;
	bool ret = table_blog->GetOne(&blog);
	if(ret == false){
		printf("GetOneBlog select from database failed!\n");
		rsp.status = 500;
		return ;
	}
	rsp.set_content(writer.write(blog),"application/json");
	rsp.status = 200;
	return;
}

//标签业务处理
void InsertTag(const Request &req,Response &rsp){
	Json::Value tag;
	Json::Reader reader;
	bool ret = reader.parse(req.body,tag);
	if(ret == false){
		printf("InsertTag parse into database failed!\n");
		rsp.status = 400;
		return ;
	}
	ret = table_tag->Insert(tag);
	if(ret == false){
		printf("Insert tag into database failed!\n");
		rsp.status = 500;
		return ;
	}
	rsp.status = 200;
	return ;
}

void DeleteTag(const Request &req,Response &rsp){
	int tag_id = std::stoi(req.matches[1]);
	bool ret = table_tag->Delete(tag_id);
	if(ret == false){
		printf("Delete tag from database failed!\n");
		rsp.status = 500;
		return ;
	}
	rsp.status = 200;
	return ;
}

void UpdateTag(const Request &req,Response &rsp){
	Json::Value tag;
	Json::Reader reader;
	Json::FastWriter writer;
	Json::Value errmsg;
	bool ret = reader.parse(req.body,tag);
	if(ret == false){
		printf("UpdateTag parse tag json failed!\n");
		errmsg["ok"] = false;
		errmsg["reason"] = "parse tag json failed!";
		rsp.set_content(writer.write(errmsg),"application/json");
		rsp.status = 400;
		return ;
	}
	int tag_id = std::stoi(req.matches[1]);
	tag["id"] = tag_id; 
	ret = table_tag->Update(tag);
	if(ret == false){
		printf("UpdateTag update failed!\n");
		rsp.status = 500;
		return ;
	}
	
	rsp.status = 200;
	return ;
}

void GetListTag(const Request &req,Response &rsp){
	Json::Value tags;
	Json::FastWriter writer;
	bool ret = table_tag->GetList(&tags);
	if(ret == false){
		printf("GetListTag select from database failed!\n");
		rsp.status = 500;
		return ;
	}
	rsp.set_content(writer.write(tags),"application/json");
	rsp.status = 200;
	return ;
}

void GetOneTag(const Request &req,Response &rsp){
	Json::Value tag;
	Json::FastWriter writer;
	int tag_id = std::stoi(req.matches[1]);
	tag["id"] = tag_id;
	bool ret = table_tag->GetOne(&tag);
	if(ret == false){
		printf("GetOneTag select from database failed!\n");
		rsp.status = 500;
		return ;
	}
	rsp.set_content(writer.write(tag),"application/json");
	rsp.status = 200;
	return ;
}
 
int main()
{
	MYSQL *mysql = blog_system::MysqlInit();
	table_blog = new blog_system::TableBlog(mysql);
	table_tag = new blog_system::TableTag(mysql);

	Server server; //实例server对象
	//为什么设置相对根目录：当客户端请求静态文件资源时，httplib会直接根据路径读取文件数据进行响应
	//服务器默认加载
	// /index.html -> ./www/index.html
	server.set_base_dir("./www");//设置url中的资源相对根目录，并且在请求时自动添加index.html
	
	//根据不同的请求格式，去判断响应相应的函数
	//博客信息的增删改查
	server.Post("/blog",InsertBlog);
	//正则表达式：R"()" 表示取出括号中所有字符的特殊含义,\d 表示匹配数字字符
	// +- 表示匹配前边的字符一次或多次 （）表示为了临时保存匹配的数据
	// /blog/(\d+)表示匹配以 /blog/ 开头，后面跟了一个数字的字符串格式，并保存后面的数字
	server.Delete(R"(/blog/(\d+))",DeleteBlog);
	server.Put(R"(/blog/(\d+))",UpdateBlog);
	server.Get("/blog",GetListBlog);
	server.Get(R"(/blog/(\d+))", GetOneBlog);

	//标签信息的增删查改
	server.Post("/tag",InsertTag);
	server.Delete(R"(/tag/(\d+))",DeleteTag);
	server.Put(R"(/tag/(\d+))",UpdateTag);
	server.Get("/tag",GetListTag);
	server.Get(R"(/tag/(\d+))", GetOneTag);
	
	server.listen("192.168.21.136",8000);
	return 0;
}





























