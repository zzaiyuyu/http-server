#!/usr/bin/python3
#-*- coding:utf-8 -*-
import os 
import re
import show
import craw

print("HTTP/1.0 200 OK")
print("Content-Type: text/html")
print("")
print('''<html><meta charset="utf-8"><head></head><body>''')

if os.getenv("METHOD") == 'GET':
    pass
    # print(os.getenv("QUERY_STRING"))
else:
    length = int(os.getenv("CONTENT_LENGTH"))
    #读的是byte流
    arg = os.read(0, length).decode('utf=8')
    print(arg)
    arg = re.split('[&=]', arg)
    mid = int(arg[1])
    page = int(arg[3])

path = "/comments/" + str(mid)
#print(path)
print('''<a href="%s">下载</a>'''%(path))

craw.craw(mid, page)
show.show_table(mid)


print("</body></html>")


