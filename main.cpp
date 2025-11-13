#include "shard.hpp"
#include <memory>

int main( int argc, char **argv ){
	if (argc > 1){
		auto shard = std::make_shared<Shard>(argv[1]);
	 	shard->run();	
	}else{
		auto shard = std::make_shared<Shard>("");
			 	shard->run();	
	};
	return 0;
};
