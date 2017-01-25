##########################################################################################
### Config Boost v1.57
##########################################################################################
set(BOOST_ROOT "" CACHE FILEPATH "Boost root path")
if(BOOST_ROOT)
    find_package(Boost COMPONENTS FileSystem system thread chrono regex REQUIRED)
    if(Boost_FOUND) 
		 include_directories(${Boost_INCLUDE_DIR})	   
    endif(Boost_FOUND)   
endif(BOOST_ROOT)