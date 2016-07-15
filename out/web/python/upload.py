import shutil
from python_handler import *


def safe (s):
    if s == None: return ""
    return s

http_context.request.process()
_files = http_context.request.files
filesLen = len(_files)

filesInfo = "Files count: %d<br />" % filesLen
if filesLen > 0:
    for kv in _files:
        f = kv.data()
        if len(f.file_name) > 0: 
            filesInfo += "Field name: %s, file name: %s, size: %d, path: %s<br />" \
                % (f.name, f.file_name, f.size, f.upload_path)
            shutil.copyfile (f.upload_path, f.upload_path + "~")
                
            
        
template = """<html><head>
<title>Upload test</title>
<style> 
    BODY { padding: 10px; margin: 10px; } 
    BODY, INPUT, SELECT, TEXTAREA {font: 10pt Tahoma, Arial; color: #000;}
    H1 {font-size: 12pt; font-weight: bold; } 
    HR {height:1px; border: 1px solid #333; color: #333;} 
    TABLE {font-size: 100%%;} 
</style></head>
<body>
    <b>Files info</b><br/>
    %s
 <hr />
 <form method="post" enctype="multipart/form-data">
    <input type="file" name="file1"/><br />
    <input type="file" name="file2"/><br />
    <input type="text" name="text1" value="%s"/><br />
    <input type="text" name="text2" value="%s"/><br />
    <input type="submit">
 </form>
 </body></html>""" % ( filesInfo, 
    safe (http_context.request["text1"]),
    safe (http_context.request["text2"]),)
   
http_context.write ( template )

   
    
