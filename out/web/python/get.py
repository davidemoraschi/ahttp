import os
from python_handler import *

http_context.write ("<b>Client info:</b><br/>")
http_context.write ( "IP: %s<br />" % http_context.client_ip)
http_context.write ( "port: %d<br />" % http_context.client_port)
http_context.write ( "User-Agent: %s<br />" % http_context.request_header.user_agent)

http_context.write ("<b>Server info:</b><br/>")
http_context.write ( "port: %d<br />" % http_context.server_port)

http_context.write ("<br/><b>Request headers:</b><br/>")
for k in http_context.request_header.items:
    http_context.write ( str( k ) + "<br />")

http_context.write ("<br/><b>GET parameters:</b><br/>")    
_get = http_context.request.get

for p in _get:
   http_context.write ( "%s: %s<br />" % (p.key(), p.data()) )
    
    
