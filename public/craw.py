#!/usr/bin/python3
#-*- coding:utf-8 -*-

import base64
import codecs
import json
import os
import pymysql
import requests
from Crypto.Cipher import AES

class Spider():
    def __init__(self, mid, page):
        self.header = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/49.0.2623.221 Safari/537.36 SE 2.X MetaSr 1.0',
            'Referer': 'http://music.163.com/'}
        self.url = 'http://music.163.com/weapi/v1/resource/comments/R_SO_4_' + str(mid) + '?csrf_token='
        self.maxPage = page
        self.id = mid

    def __get_jsons(self, url, page):
        # 获取两个参数
        music = WangYiYun()
        text = music.create_random_16()
        params = music.get_params(text, page)
        encSecKey = music.get_encSEcKey(text)
        fromdata = {'params': params, 'encSecKey': encSecKey}
        jsons = requests.post(url, data=fromdata, headers=self.header)
        # print(jsons.raise_for_status())
        # 打印返回来的内容，是个json格式的
        # print(jsons.text)
        return jsons.text

    def json2list(self, jsons):
        '''把json转成字典，并把他重要的信息获取出来存入列表'''
        # 可以用json.loads()把他转成字典
        # print(json.loads(jsons.text))
        users = json.loads(jsons)
        comments = []
        myComments = []
        for user in users['comments']:
            # print(user['user']['nickname']+' : '+user['content']+'   点赞数：'+str(user['likedCount']))
            name = user['user']['nickname']
            content = user['content']
            likedCount = user['likedCount']
            user_dict = {'name': name, 'content': content, 'likedCount': likedCount}
            comments.append(user_dict)
        return comments

    def write2sql(self, comments):
        '''把评论写入数据库'''
        music = Operate_SQL()
        #print('第%d页正在获取' % self.page)
        for comment in comments:
            # print(comment)
            music.add_data(comment, self.id)
        #print('   该页获取完成')

    # 把数据库存好的数据导出成txt
    def derive2txt(self):
        save = Operate_SQL()
        save.save2txt()

    def run(self):
        self.page = 1
        music = Operate_SQL()
        music.create_table(self.id)
        while True:
            jsons = self.__get_jsons(self.url, self.page)
            comments = self.json2list(jsons)
            # 当这一页的评论数少于20条时，证明已经获取完.或者达到指定页数
            self.write2sql(comments)
            if len(comments) < 20 or self.page > self.maxPage - 1 or self.page > 100:
                # 保存为txt
                #self.derive2txt()
                break
            self.page += 1

# 找出post的两个参数params和encSecKey
class WangYiYun():
    def __init__(self):
        # 在网易云获取的三个参数
        self.second_param = '010001'
        self.third_param = '00e0b509f6259df8642dbc35662901477df22677ec152b5ff68ace615bb7b725152b3ab17a876aea8a5aa76d2e417629ec4ee341f56135fccf695280104e0312ecbda92557c93870114af6c9d05c4f7f0c3685b7a46bee255932575cce10b424d813cfe4875d3e82047b97ddef52741d546b8e289dc6935b3ece0462db0a22b8e7'
        self.fourth_param = '0CoJUm6Qyw8W8jud'

    def create_random_16(self):
        '''获取随机十六个字母拼接成的字符串'''
        return (''.join(map(lambda xx: (hex(ord(xx))[2:]), str(os.urandom(16)))))[0:16]

    def aesEncrypt(self, text, key):
        # 偏移量
        iv = '0102030405060708'
        # 文本
        pad = 16 - len(text) % 16
        text = text + pad * chr(pad)
        encryptor = AES.new(key, 2, iv)
        ciphertext = encryptor.encrypt(text)
        ciphertext = base64.b64encode(ciphertext)
        return ciphertext

    def get_params(self, text, page):
        '''获取网易云第一个参数'''
        # 第一个参数
        if page == 1:
            self.first_param = '{rid: "R_SO_4_400162138", offset: "0", total: "true", limit: "20", csrf_token: ""}'
        else:
            self.first_param = (
                '{rid: "R_SO_4_400162138", offset:%s, total: "false", limit: "20", csrf_token: ""}' % str(
                    (page - 1) * 20))

        params = self.aesEncrypt(self.first_param, self.fourth_param).decode('utf-8')
        params = self.aesEncrypt(params, text)
        return params

    def rsaEncrypt(self, pubKey, text, modulus):
        '''进行rsa加密'''
        text = text[::-1]
        rs = int(codecs.encode(text.encode('utf-8'), 'hex_codec'), 16) ** int(pubKey, 16) % int(modulus, 16)
        return format(rs, 'x').zfill(256)

    def get_encSEcKey(self, text):
        '''获取第二个参数'''
        pubKey = self.second_param
        moudulus = self.third_param
        encSecKey = self.rsaEncrypt(pubKey, text, moudulus)
        return encSecKey

# 操作 mysql
class Operate_SQL():
    # 连接数据库
    def __get_conn(self):
        try:
            self.conn = pymysql.connect(host='127.0.0.1', user='root', passwd='yuyu0912', 
                                            port=3306, db='music', charset='utf8mb4')
        except Exception as e:
            print(e, '数据库连接失败')

    def __close_conn(self):
        '''关闭数据库连接'''
        try:
            if self.conn:
                self.conn.close()
        except pymysql.Error as e:
            print(e, '关闭数据库失败')

    def add_data(self, comment, id):
        '''增加一条数据到数据库'''
        sql = 'INSERT INTO `%s` (`name`,`content`,`likedCount`) VALUE("%s","%s",%s)'% (id, comment['name'], comment['content'], comment['likedCount'])
        try:
            self.__get_conn()
            cursor = self.conn.cursor()
            cursor.execute(sql)
            self.conn.commit()
            return 1
        except AttributeError as e:
            print(e, '添加数据失败')
            # 添加失败就倒回数据
            self.conn.rollback()
            return 0
        except pymysql.DataError as e:
            print(e)
            self.conn.rollback()
            return 0
        finally:
            if cursor:
                cursor.close()
            self.__close_conn()

    def create_table(self, id):
        sql = '''create table `%s` (  `id` int PRIMARY KEY auto_increment,
                                    `name` varchar(30) DEFAULT NULL,
                                   `content` varchar(200) DEFAULT NULL,
                                   `likedCount` int(11) DEFAULT NULL
                                  ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4''' % (id)
        try:
            self.__get_conn()
            cursor = self.conn.cursor()
            cursor.execute(sql)
            self.conn.commit()
            return 1
        except AttributeError as e:
            print(e, '添加数据失败')
            # 添加失败就倒回数据
            self.conn.rollback()
            return 0
        except pymysql.DataError as e:
            print(e)
            self.conn.rollback()
            return 0
        finally:
            if cursor:
                cursor.close()
            self.__close_conn()

    def save2txt(self):
        sql = '''select * from %s into outfile "%s" LINES TERMINATED BY '\r\n';''' % ("comments", r"D:\xxx.txt")
        #print(sql)
        try:
            self.__get_conn()
            cursor = self.conn.cursor()
            cursor.execute(sql)
            self.conn.commit()
            print("存储成功")
            return 1
        except Exception as e:
            print(e)
            self.conn.rollback()
            return 0
        finally:
            if cursor:
                cursor.close()
            self.__close_conn()
# 得到歌曲id，爬去页数,生成txt目录
def craw(mid, page):
    spider = Spider(mid, page)
    spider.run()

if __name__ == '__main__':
    mid = int(input())
    page = int(input())
    craw(mid, page)

