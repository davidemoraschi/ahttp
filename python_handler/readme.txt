========================================================================
    python_handler Project Overview
========================================================================

Python script retrieve HttpContext wrapper object registered in globals as "http_context"

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////

Boost 1.34.1 sources update can be required - boost\python\to_python_converter.hpp:
 
template <class T, class Conversion>
to_python_converter<T,Conversion>::to_python_converter()
{
...
	converter::registration const* reg = converter::registry::query( type_id<T>());
 
    if ( 0 == reg->m_to_python ) 
    {
		converter::registry::insert(
			&normalized::convert,
			type_id<T>());
	};
...
}


/////////////////////////////////////////////////////////////////////////////
