#include<iostream>
#include<mutex>
#include<mysql/mysql.h>
#include<jsoncpp/json/json.h>

#define MYSQL_HOST "127.0.0.1"
#define MYSQL_USER "root"
#define MYSQL_PSWD "111111"
#define MYSQL_DB "db_blog"
namespace blog_system{

	static std::mutex b_mutex;
//对外提供接口返回初始化完成的mysql句柄
//初始化
MYSQL *MysqlInit(){
	MYSQL *mysql;
	mysql = mysql_init(NULL);
	if(mysql == NULL){
		printf("init mysql error\n");
                return NULL;
	}
	//连接服务器
	if(mysql_real_connect(mysql,MYSQL_HOST,MYSQL_USER,MYSQL_PSWD,NULL,0,NULL,0) == 0){
		printf("connect mysql error:%s\n",mysql_error(mysql));
		mysql_close(mysql);
		return NULL;
	}
	//设置字符集
	if(mysql_set_character_set(mysql,"utf8") != 0){
		printf("set client character  error:%s\n",mysql_error(mysql));
		mysql_close(mysql);	
		return NULL;
	}
	//选择数据库
	if(mysql_select_db(mysql,MYSQL_DB) != 0){
		printf("select db  error:%s\n",mysql_error(mysql));
		mysql_close(mysql);
		return NULL;
	}
	return mysql;
}
//删除句柄
void MysqlRelease(MYSQL *mysql){
	if(mysql){
	 mysql_close(mysql);
	}
	//return;
}
//执行语句的共有接口
bool MysqlQuery(MYSQL *mysql,const char* sql){
	int ret = mysql_query(mysql,sql);
	if(ret  != 0){
		printf("query sql:[%s] failed:%s\n",sql,mysql_error(mysql));
		return false;
	}
	return true;
}

//外部通过TableBlog类提供的接口实现功能
class TableBlog{
public:
	TableBlog(MYSQL *mysql):_mysql(mysql){}
	//从blog中取出博客信息，组织sql语句，将数据插入数据库
bool Insert(Json::Value &blog){
	#define INSERT_BLOG "insert tb_blog values(null,'%d','%s','%s',now());"
	//博客正文长度可能会导致越界，所以不能定义tmp的长度固定
	int len = blog["content"].asString().size() + 4096;
	char *tmp = (char*)malloc(len);
	//博客id不需要手动输入，因为自增属性 用于整型数据字段
	//表示每次添加数据时该字段的值默认从1开始自动+1
	sprintf(tmp,INSERT_BLOG,blog["tag_id"].asInt(),
		blog["title"].asCString(),
		blog["content"].asCString());
	bool ret = MysqlQuery(_mysql,tmp);
	free(tmp);
	return ret;
}
	
	//根据博客id删除对应博客
bool Delete(int blog_id){
	#define DELETE_BLOG "delete from tb_blog where id=%d"
	char tmp[1024] = {0};
	sprintf(tmp,DELETE_BLOG,blog_id);
	bool ret = MysqlQuery(_mysql,tmp);
	
	return ret;
}
	
	//从blog中取出博客信息，组织sql语句，更新数据库
bool Update(Json::Value &blog){
	#define UPDATE_BLOG "update tb_blog set tag_id=%d,title='%s',content='%s' where id=%d;"
	int len = blog["content"].asString().size() + 4096;
	char *tmp = (char*)malloc(len);
	sprintf(tmp,UPDATE_BLOG,blog["tag_id"].asInt(),blog["title"].asCString(),blog["content"].asCString(),blog["id"].asInt());
	bool ret = MysqlQuery(_mysql,tmp);
	free(tmp);
	return ret;
}
	//通过blog返回所有博客列表，不包含正文
bool GetList(Json::Value *blogs){
	#define GETLIST_BLOG "select id,tag_id,title,ctime from tb_blog;"
	b_mutex.lock();
	//执行查询语句
	bool ret = MysqlQuery(_mysql,GETLIST_BLOG);
	if(ret == false){
		b_mutex.unlock();
		return false;
	}
	//保存结果集
	MYSQL_RES *res = mysql_store_result(_mysql);
	b_mutex.unlock();
	if(ret == false){
		printf("store all blog result failed:%s\n",mysql_error(_mysql));
		return false;
	}
	//遍历结果集
	int row_num = mysql_num_rows(res);
	for(int i = 0;i<row_num;i++){
		MYSQL_ROW row = mysql_fetch_row(res);
		Json::Value blog;
		blog["id"] = std::stoi(row[0]);
		blog["tag_id"] = std::stoi(row[1]);
		blog["title"] = row[2];
		blog["ctime"] = row[3];
		blogs->append(blog); //添加json数组元素
	}
	mysql_free_result(res);
	return true;
}
	//返回单个博客信息，包含正文
bool GetOne(Json::Value *blog){
      #define GETONE_BLOG "select tag_id,title,content,ctime from tb_blog where id=%d;"
	char tmp[1024] = {0};
	sprintf(tmp,GETONE_BLOG,(*blog)["id"].asInt());
	b_mutex.lock();
	bool ret = MysqlQuery(_mysql,tmp);
	if(ret == false){
		b_mutex.unlock();
		return false;
	}
	MYSQL_RES *res = mysql_store_result(_mysql);
	b_mutex.unlock(); 
	if(res == NULL){
		printf("store blog result failed:%s\n",mysql_error(_mysql));
		return false;
	}
	int row_num = mysql_num_rows(res);
	//获取单个博客信息所以row_num == 1
	if(row_num != 1){
		printf("get one blog result error\n");
		mysql_free_result(res);
		return false;
	}
	MYSQL_ROW row = mysql_fetch_row(res);
	(*blog)["tag_id"] = std::stoi(row[0]);
	(*blog)["title"] = row[1];
        (*blog)["content"] = row[2];
	(*blog)["ctime"] = row[3];
        
	mysql_free_result(res);
	return true;



}
private:
	MYSQL *_mysql;
};

class TableTag{
public:
	TableTag(MYSQL *mysql):_mysql(mysql){}
bool Insert(Json::Value &tag){
	#define INSERT_TAG "insert tb_tag values (null,'%s');"
	char tmp[1024] = {0};
	sprintf(tmp,INSERT_TAG,tag["name"].asCString());
	return MysqlQuery(_mysql,tmp);
}
bool Delete(int tag_id){
	#define DELETE_TAG "delete from tb_tag where id=%d;"
	char tmp[1024] = {0};
	sprintf(tmp,DELETE_TAG,tag_id);
	return MysqlQuery(_mysql,tmp);
}
bool Update(Json::Value &tag){
	#define UPDATE_TAG "update tb_tag set name='%s' where id=%d;"
	char tmp[1024] = {0};
	sprintf(tmp,UPDATE_TAG,tag["name"].asCString(),tag["id"].asInt());
	return MysqlQuery(_mysql,tmp);
}
bool GetList(Json::Value *tags){
	#define GETLIST_TAG "select id,name from tb_tag;"
	b_mutex.lock();
	bool ret = MysqlQuery(_mysql,GETLIST_TAG);
	if(ret == false){
		b_mutex.unlock();
		return false;
	}
	MYSQL_RES *res = mysql_store_result(_mysql);
	b_mutex.unlock();
	if(res == NULL){
		printf("store all tag result failed:%s",mysql_error(_mysql));
		return false;
	}
	int row_num = mysql_num_rows(res);
	for(int i=0;i<row_num;i++){
		MYSQL_ROW row = mysql_fetch_row(res);
		Json::Value tag;
		tag["id"] = std::atoi(row[0]);
		tag["name"] = row[1];
		tags->append(tag);
	}
	mysql_free_result(res);
	return true;
}
bool GetOne(Json::Value *tag){
	#define GETONE_TAG "select name from tb_tag where id=%d;"
	char tmp[1024] = {0};
	sprintf(tmp,GETONE_TAG,(*tag)["id"].asInt());
	b_mutex.lock();
	bool ret = MysqlQuery(_mysql,tmp);
	if(ret == false){
		b_mutex.unlock();
		return false;
	}
	MYSQL_RES *res = mysql_store_result(_mysql);
	b_mutex.unlock();
	if(res == NULL){
		printf("store all tag result failed:%s",mysql_error(_mysql));
		return false;
	}
	int row_num = mysql_num_rows(res);
	if(row_num != 1){
		printf("get one tag result error:%s\n",mysql_error(_mysql));
	}
	MYSQL_ROW row = mysql_fetch_row(res);
	(*tag)["name"] = row[0];
	mysql_free_result(res);
	return true;	
}

private:
	MYSQL *_mysql;	
};





}
