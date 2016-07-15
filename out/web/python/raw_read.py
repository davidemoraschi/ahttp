import os
from python_handler import *

requestContent = http_context.request.raw_read(1024)

template = """<html><head>
<title>POST test</title>
<style> 
    BODY { padding: 10px; margin: 10px; } 
    BODY, INPUT, SELECT, TEXTAREA {font: 10pt Tahoma, Arial; color: #000;}
    H1 {font-size: 12pt; font-weight: bold; } 
    HR {height:1px; border: 1px solid #333; color: #333;} 
    TABLE {font-size: 100%%;} 
</style></head>
<body>
    Request content: "%s"
 <hr />
 <form method="post">
    <textarea name="text" rows="20"></textarea><br />
    <input type="submit">
 </form>
 </body></html>""" % ( escape_html (requestContent), )
   
http_context.write ( template )

   
    
