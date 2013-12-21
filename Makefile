all:
	cd src; make
	cd http-root-dir/cgi-src; make

clean:
	rm ./build/* 
	rm ./bin/*
