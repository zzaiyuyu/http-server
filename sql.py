#!/usr/bin/python3
#-*- coding:utf-8 -*-
'''Zheng 's BUG'''
from bs4 import BeautifulSoup
from urllib.request import urlopen
from urllib.error import  HTTPError
import random
import re
from urllib.parse import quote
import os
import string
import pymysql
baseUrl = "https://baike.baidu.com"

def getLinks(link):
    try:
        link = quote(link, safe= string.printable)                       ##读取中英混编的url
        page = urlopen(baseUrl + link).read().decode("utf-8")
    except HTTPError as e:
        return "https://baike.baidu.com"
    bsObj = BeautifulSoup(page, "html.parser")
    return bsObj.findAll("a", href=re.compile("^(/item/)((?!:).)*$"))  ##内层嵌套找不到???????


def getSum(newPage):
    try:
        newPage = quote(newPage, safe=string.printable)  #处理中文URL
        newPage = urlopen(baseUrl + newPage).read().decode("utf-8")
    except HTTPError as e:
        return None
    bsObj = BeautifulSoup(newPage, "html.parser")
    #print(bsObj.body.h1.get_text())
    title = bsObj.body.h1.get_text()
    content = ""
    if bsObj.find("div", {"class": "lemma-summary"}) is not None:
        for cont in bsObj.find("div", {"class":"lemma-summary"}).findAll("div", {"class":"para"}):
            if cont is not None:
                #print(con.get_text())
                content += cont.get_text()
        store(title, content)

def store(title, content):
    cur.execute("insert into pages (title, summary) values (\"%s\",\"%s\")", (title, content))
    cur.connection.commit()


link = getLinks("")
conn = pymysql.connect(host='127.0.0.1', port=3306,
                       user='root', passwd='yuyu0912', db='mysql', charset="utf8")
cur = conn.cursor()
cur.execute("use baidu")
os.getenv()
getSum()
cur.close()
conn.close()
