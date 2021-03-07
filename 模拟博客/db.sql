create database if not exists db_blog;
use db_blog;

drop table if exists tb_tag;
create table if not exists tb_tag(
	id int primary key auto_increment comment '标签ID',
	name varchar(32) comment '标签名称'
);

drop table if  exists tb_blog;
create table if not exists tb_blog(
    id int primary key auto_increment comment '博客ID',
     tag_id int comment '标签ID',
     title varchar(255) comment '博客标题',
     content text comment '博客正文',
     ctime datetime comment '博客创建时间'
);

insert tb_tag values(null,'C++'),(null,'java'),(null,'go');
insert tb_blog values(null,1,'一篇C++博客','*正文*', now()),(null,2,'一篇java博客','*正文*',now()),(null,3,'一篇go博客','*正文*',now());
  
