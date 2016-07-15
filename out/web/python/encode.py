 # -*- coding: utf-8 -*-
 
import python_handler
http_context.set_utf8_html_response()

encoded = python_handler.encode_url_part ("test: тест")
http_context.write ("<b>Encoded</b>: %s<br/>" % encoded)
http_context.write ("<b>Decoded</b>: %s" % python_handler.decode_url (encoded) )

http_context.flush ()
http_context.set_utf8_html_response()


