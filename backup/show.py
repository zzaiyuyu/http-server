#!/usr/bin/python3
#-*- coding:utf-8 -*-

import pymysql
# 爬取
# 存储
# 展示
def show_table(mid):
    try:
        conn = pymysql.connect(host='127.0.0.1', user='root', passwd='yuyu0912', port=3306, 
                                db='music',  charset='utf8mb4')
        sql = "select * from `%s`;"%(mid)
        cur = conn.cursor()
        cur.execute(sql)
        conn.commit()
        #返回的是一个二元元组
        val = cur.fetchall()
        row = len(val)
        col = len(val[0])
        print("以下是%d条数据"%(row))
        print('''<style>
                .table7_7 table {
                    width:100%;
                        margin:15px 0
                        }
        .table7_7 th {
            background-color:#00A5FF;
                background:-o-linear-gradient(90deg, #00A5FF, #6dcbfe);
                    background:-moz-linear-gradient( center top, #00A5FF 5%, #6dcbfe 100% );
                        background:-webkit-gradient( linear, left top, left bottom, color-stop(0.05, #00A5FF), color-stop(1, #6dcbfe) );
                            filter:progid:DXImageTransform.Microsoft.gradient(startColorstr='#00A5FF', endColorstr='#6dcbfe');
                                color:#FFFFFF
                                }
        .table7_7,.table7_7 th,.table7_7 td
        {
            font-size:0.95em;
                text-align:center;
                    padding:4px;
                        border-bottom:1px solid #efefef;
                            border-collapse:collapse
                            }
        .table7_7 tr:nth-child(odd){
            background-color:#aae1fe;
                background:-o-linear-gradient(90deg, #aae1fe, #eef9fe);
                    background:-moz-linear-gradient( center top, #aae1fe 5%, #eef9fe 100% );
                        background:-webkit-gradient( linear, left top, left bottom, color-stop(0.05, #aae1fe), color-stop(1, #eef9fe) );
                            filter:progid:DXImageTransform.Microsoft.gradient(startColorstr='#aae1fe', endColorstr='#eef9fe');
                            }
        .table7_7 tr:nth-child(even){
            background-color:#fdfdfd;
            }
        </style>''')
        print('<table class="table7_7">')
        for r in range(0, row):
            print('<tr>')
            for c in range(0, col):
                print('<td>', end='')
                print(val[r][c], end='')
                print('</td>')
            print('</tr>')
        print('</table>')
    except Exception as e:
        print(e, '数据库连接失败')
