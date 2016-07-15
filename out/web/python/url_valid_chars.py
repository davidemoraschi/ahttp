
chars = []
for i in range(0, 255):
    ch = chr(i)
    if (ch >= 'a') and (ch <= 'z'):
        chars.append ("true")
    elif (ch >= 'A') and (ch <= 'Z'):
        chars.append ("true")
    elif (ch >= '0') and (ch <= '9'):
        chars.append ("true")
    elif (ch == '.') or (ch == '-') or (ch == '_') or (ch == '\''):
        chars.append ("true")
    else:
        chars.append ("false")


for i in range(0, 255):
    if (i % 10) == 0:
        http_context.write ("<br/>")
    http_context.write ( chars[i] + "," )
