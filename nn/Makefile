BASE_PATH := /ebs_data1/bob_dev_group/base
COMM_PATH := ../../../common
INC_PATH := $(BASE_PATH) $(COMM_PATH)/aws_comm $(COMM_PATH)/aws_tbl $(COMM_PATH)/comm $(COMM_PATH)/reg_comm ./bussiness ./frame
SRC_FILES := $(COMM_PATH)/aws_comm/*.cpp $(COMM_PATH)/aws_tbl/*.cpp $(COMM_PATH)/comm/*.cpp $(COMM_PATH)/reg_comm/*.cpp ./bussiness/*.cpp ./frame/*.cpp

LIB_PATH := $(BASE_PATH)/lib
project.targets += report_center
project.extra_warning := -Wno-write-strings -fpermissive -Wno-unused-but-set-variable
project.optimize_flags :=
report_center.path := ../bin
report_center.name := report_center
report_center.defines := TIXML_USE_STL
report_center.sources := $(wildcard $(SRC_FILES))
report_center.includes := $(INC_PATH) .
report_center.ldadd := $(LIB_PATH)/libzookeeper_mt.a $(LIB_PATH)/libEncode.a $(LIB_PATH)/libSvrPub.a $(LIB_PATH)/libWtseBase.a $(LIB_PATH)/libjsoncpp.a -lpthread -lcurl

include $(BASE_PATH)/Generic.mak

