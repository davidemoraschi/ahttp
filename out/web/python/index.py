import os
from python_handler import *

template = """
<html>
<head>
	<title>INDEX</title>
	<style>
		 BODY {	font-family: "Trebuchet MS", Arial, Helvetica; background-color: #ffffff; font-size: 10pt;}
		 TABLE { font-size: 100%;}
		 A {color: #ff9933; text-decoration : underline;}
	</style>
</head>

<body style="padding: 10;" >
<b>Examples:</b><br/>
{body}
</body>
</html>
"""

curDir = os.path.split (http_context.script_path)[0]
body = ""

for fname in [os.path.normcase (f) for f in os.listdir (curDir)]:
    body += "<a href=\"%s\">%s</a></br>" % (fname, fname)

http_context.write ( template.replace("{body}", body) )