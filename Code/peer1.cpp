#include <unistd.h> 
#include <bits/stdc++.h>
#include <stdio.h> 
#include <arpa/inet.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <thread>
#include <openssl/sha.h>

#define SOCKET_BUFF 16384
#define CHUNK_SIZE 524288

using namespace std;
string current_login = "";
string serverip;
int serverport;
string trackerip;
int trackerport;
map<string,set<int>> chunk_map_per_file;
map<string,string> file_to_file_path;
map<pair<string,string>,vector<int>> socket_to_chunks; //filename ip:port chunks
map<string,string> downloaded_files;
vector<string> split(string s, char delemitor){
    stringstream ss(s);
    vector<string>a;
    string temp;
    while(getline(ss,temp,delemitor))
    {
        a.push_back(temp);
    }
    return a;
}


void serverfunc(int sd,int i){
    int valread;
	char buffer[SOCKET_BUFF];
	if((valread = read( sd , buffer, SOCKET_BUFF)) > 0){
		// std::cout << buffer << endl;
	}
	vector<string> req_vec = split(string(buffer),',');
	bzero(buffer,SOCKET_BUFF);

	if(req_vec[0] == "bitvector"){
		//bitvector filename
		string filename = req_vec[1];
		string response(SOCKET_BUFF,'\0');
		response = "";
		for(auto a : chunk_map_per_file[filename]){
			response+=to_string(a)+",";
		}
		// std::cout << response<<endl;
		send(sd , response.c_str() , response.capacity() , 0 );
	}
	if(req_vec[0] == "downloadchunks"){
		
		string filename = req_vec[1];
		int no_of_chunks = atoi(req_vec[2].c_str());
		while(no_of_chunks--){
			int chunkno;
			if((valread = read( sd , buffer, SOCKET_BUFF)) > 0){
				// std::cout << buffer << endl;
				chunkno = atoi(buffer);
				bzero(buffer,SOCKET_BUFF);

			}
			
			// cout << "jabra" << endl;
			string filepath = file_to_file_path[filename];
			FILE * f = fopen(filepath.c_str(),"r");
			fseek(f,chunkno*CHUNK_SIZE,SEEK_SET);
			int size = CHUNK_SIZE;
			while( size > 0){
				((valread = fread( buffer,sizeof(char), SOCKET_BUFF,f)) > 0);
				send(sd , buffer, SOCKET_BUFF , 0 );
				
				bzero(buffer,SOCKET_BUFF);
				// cout << "Stuck here"<<endl;
				size-=SOCKET_BUFF;

			}
			bzero(buffer,SOCKET_BUFF);
			// std::cout << "Sent :"<< filename <<" chunk no:" << chunkno  <<endl;
			
			
		}
	}
	else{
        string response(SOCKET_BUFF,'\0');
		 response="wrong request";
		send(sd , response.c_str() , response.capacity() , 0 );	
	}
	
}


void serverstart(string serverip, int port){
	int server_fd, new_socket, valread; 
	struct sockaddr_in address; 
    int opt = 1; 
    socklen_t addrlen = sizeof(address); 
	 
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
	{ 
		perror("socket failed"); 
		exit(EXIT_FAILURE); 
	} 
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
	{
		perror("setsockopt Error");
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = inet_addr(serverip.c_str());
	address.sin_port = htons( port ); 
	
	
	if (bind(server_fd, (struct sockaddr *)&address, 
								sizeof(address))<0) 
	{ 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	if (listen(server_fd, 20) < 0) 
	{ 
		perror("listen"); 
		exit(EXIT_FAILURE); 
	} 
	int i = 0;
    while(new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)){
        i++;
		if ((new_socket)<0) { 
				perror("accept"); 
				exit(EXIT_FAILURE); 
		}
		else{
			thread serverreq(serverfunc,new_socket,i);
			serverreq.join();
			
		}
    
    } 
}
void peer_chunk_download(string peer_server_ip, int port, string filename,string filepath, vector<int> chunks_to_download){
	int peer_server_port  = port;
	int sock = 0, valread; 
	struct sockaddr_in serv_addr; 
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{ 
		printf("\n Socket creation error \n"); 
		return ; 
	} 

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(peer_server_port); 
	
	if(inet_pton(AF_INET, peer_server_ip.c_str(), &serv_addr.sin_addr)<=0) 
	{ 
		printf("\nInvalid address/ Address not supported \n"); 
		return ; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
	{ 
		perror("Connection Failed "); 
		return ; 
	} 
	char buffer[SOCKET_BUFF];
	string server_req(SOCKET_BUFF,'\0');
	server_req="downloadchunks,"+filename+","+ to_string(chunks_to_download.size());
	// std::cout<<server_req<<endl;
	send(sock , server_req.c_str() , server_req.capacity() , 0 );

	fstream out(filepath+"/"+filename,ios::out|ios::in|ios::binary);
	for(auto chunkno: chunks_to_download){
	server_req=to_string(chunkno);
	// std::cout<<server_req<<endl;
	send(sock , server_req.c_str() , server_req.capacity() , 0 );
	
	out.seekp(chunkno*CHUNK_SIZE,ios::beg);
	int remaining_size=CHUNK_SIZE;
	while( remaining_size > 0){
		if((valread = read( sock , buffer, SOCKET_BUFF)) > 0) 
		{out.write(buffer,SOCKET_BUFF);
		}
		remaining_size -= SOCKET_BUFF;
		valread=0;
		bzero(buffer,SOCKET_BUFF);
	}
	// std::cout << "Downloaded chunk" << chunkno<<endl;
	}
	
	out.close();
	

}
void download_bit_vector(string peer_server_ip, int port, string filename, int filesize){
	int peer_server_port  = port;
	int sock = 0, valread; 
	struct sockaddr_in serv_addr; 
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{ 
		printf("\n Socket creation error \n"); 
		return ; 
	} 

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(peer_server_port); 
	
	if(inet_pton(AF_INET, peer_server_ip.c_str(), &serv_addr.sin_addr)<=0) 
	{ 
		printf("\nInvalid address/ Address not supported \n"); 
		return ; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
	{ 
		perror("Connection Failed "); 
		return ; 
	} 
	char buffer[SOCKET_BUFF];
	string server_req(SOCKET_BUFF,'\0');

	server_req="bitvector,"+filename;
	// std::cout<<server_req<<endl;
	send(sock , server_req.c_str() , server_req.capacity() , 0 );
	
	string response;
	if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
		// std::cout << buffer << endl;
		response = buffer;
		bzero(buffer,SOCKET_BUFF);
	}
	vector<string> res_vec = split(response,',');
	int chunks = (filesize+CHUNK_SIZE-1)/CHUNK_SIZE;
	vector<int> chunks_avail(chunks,0);
	for(auto a : res_vec){
		int chunk_no = atoi(a.c_str());
		chunks_avail[chunk_no] = 1;
	}
	// port_to_chunks[port] = chunks_avail;
	string peer_socket = peer_server_ip+":"+to_string(peer_server_port);
	socket_to_chunks[{filename,peer_socket}] = chunks_avail;
	


			
	



}
int main(int argc, char const *argv[]) 
{ 	
	if(argc != 3){
		std::cout<< "Invalid no of arguments!!"<<endl;
		std::cout<<"Format: ./peer1 <peer_server_ip>:<peer_server_port> tracker_info.txt "<<endl;
		return 0;
	}
	string serversocket = argv[1];
	vector<string> peer_socket = split(serversocket,':');
	serverip = peer_socket[0];
	serverport = atoi(peer_socket[1].c_str());
	


	string tracker_info = argv[2];
    fstream in(tracker_info,ios::in);
    string tracker;
    getline(in, tracker,'\n');
    vector<string> trackersocket = split(tracker,':');
    trackerip = trackersocket[0];
    trackerport = atoi(trackersocket[1].c_str());
	// int trackerport  = atoi(argv[2]);
	if(in.fail()){
				std::cout << argv[2]<< " not present" << endl;
				return 0;
	}
	thread server(serverstart,serverip,serverport);
	server.detach();
	int sock = 0, valread; 
	struct sockaddr_in serv_addr; 
	 
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{ 
		printf("\n Socket creation error \n"); 
		return -1; 
	} 

	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(trackerport); 
	
	if(inet_pton(AF_INET, trackerip.c_str(), &serv_addr.sin_addr)<=0) 
	{ 
		printf("\nInvalid address/ Address not supported \n"); 
		return -1; 
	} 

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
	{ 
		perror("Connection Failed "); 
		return -1; 
	} 
	while(1)
    {
		string client_message;
		cin >> client_message;
		// cout << client_message << endl;
		if(client_message == "upload_file"){


			char buffer[SOCKET_BUFF] = {0}; 
			string filepath; cin >> filepath;
			string filename;
			string groupname; cin >> groupname;
			
			 if(current_login == ""){
				std::cout << "No user login" << 	endl;
				std::cout << endl;

				continue;	
			}

			filename = split(filepath,'/').back();
			file_to_file_path[filename] = filepath;


			ifstream in(filepath,ios::ate|ios::binary);

			if(in.fail()){
				std::cout << "wrong file path or name" << endl;
				continue;
			}

			int size = in.tellg();
			in.close();
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+current_login+","+filename+","+groupname+","+to_string(size)+","+serverip+":"+to_string(serverport);

			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				std::cout << buffer << endl;
				bzero(buffer,SOCKET_BUFF);
			}
			int chunks = (size+CHUNK_SIZE-1)/CHUNK_SIZE;
			for(int i  = 0 ; i < chunks; i++)
				{
					chunk_map_per_file[filename].insert(i);
						
			}
			
			
			
		}
		else if(client_message == "stop_share"){


			char buffer[SOCKET_BUFF] = {0}; 
			// string filepath; cin >> filepath;
			string groupname; cin >> groupname;
			string filename; cin>>filename;
			
			 if(current_login == ""){
				std::cout << "No user login" << 	endl;
				std::cout << endl;

				continue;	
			}
			// for(auto a : file_to_file_path) std::cout << a.first << endl;
			if(file_to_file_path.find(filename) == file_to_file_path.end()){
				std::cout << "No such file "+filename+" shared on this machine" << endl;
				std::cout << endl;
				continue;
			}
			 

	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+current_login+","+filename+","+groupname+","+serverip+":"+to_string(serverport);

			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response = buffer;
				// std::cout << buffer << endl;
				bzero(buffer,SOCKET_BUFF);
			}

			if(response == "Stopped sharing"){
			file_to_file_path.erase(filename);
			chunk_map_per_file.erase(filename);
			}
			std::cout << response<<endl;
			
		} 
		else if(client_message == "list_files"){
			

			char buffer[SOCKET_BUFF] = {0}; 
		
			string groupname; cin >> groupname;
			
			 if(current_login == ""){
				std::cout << "No user login" << 	endl;
				std::cout << endl;

				continue;	
			}


			
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+current_login+","+groupname;

			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				std::cout << buffer << endl;
				bzero(buffer,SOCKET_BUFF);
			}
			
		}
		else if(client_message == "download_file"){
			//get the list of the peers that have the file
			
			string groupname; cin >> groupname;
			string filename; cin >> filename;
			string destination_path; cin >> destination_path;
			
			if(current_login == ""){
				std::cout << "No user login" << 	endl;
				std::cout << endl;

				continue;	
			}
			
			//check the path
			//check if file already exists

			char buffer[SOCKET_BUFF] = {0};
		

			string request(SOCKET_BUFF,'\0');
			
			request = client_message+","+current_login+","+ filename +","+ groupname;
			send(sock , request.c_str() , request.capacity() , 0 );

			string response;
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				// std::cout << buffer << endl;
				response = buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			if(response =="0"){
				// 200592,127.0.0.1:8080,127.0.0.1:9000
				if(downloaded_files.find(filename) != downloaded_files.end()){
					cout << "Already downloaded" << endl;
					continue;
				}
				downloaded_files[filename] = "D";
				string response;
				if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				// std::cout << buffer << endl;
				response = buffer;
				bzero(buffer,SOCKET_BUFF);
				}
				vector<string> res_vec = split(response,',');
				int filesize = atoi( res_vec[0].c_str());
				vector<thread> get_bv;


				//intilaise empty file
				string s(filesize,'\0');
				fstream out(destination_path+"/"+filename,ios::out|ios::binary);
				out.write(s.c_str(), s.length());
				out.close();
			
				for(int i = 1; i< res_vec.size();i++){
					string peer_socket = res_vec[i];
					vector<string> peer_socket_vector = split(res_vec[i],':');
					string peer_server_ip = peer_socket_vector[0];
					int peer_server_port = atoi(peer_socket_vector[1].c_str());
					get_bv.push_back(thread(download_bit_vector, peer_server_ip, peer_server_port, filename,filesize));
					for(auto &th: get_bv){
						if(th.joinable()) th.join();
					}
					
					// std::cout << peer_socket<<endl;
					// for(auto chunks: socket_to_chunks[{filename,peer_socket}]) std::cout << chunks <<" ";
					// std::cout << endl;
					

				}

			


			


			//get and fill chunk
			int chunks = socket_to_chunks[{filename,res_vec[1]}].size();
			map<pair<string,int>,vector<int>> download_vector;
			for(int i = 1, j =0 ; i< res_vec.size() && j < chunks; j++){
					
					string peer_socket = res_vec[i];
					vector<string> peer_socket_vector = split(res_vec[i],':');
					string peer_server_ip = peer_socket_vector[0];
					int peer_server_port = atoi(peer_socket_vector[1].c_str());
					
					
					
					
					download_vector[{peer_server_ip,peer_server_port}].push_back(j);
					

					i++;
					if(i == res_vec.size()) i=1;
					
			}
			
			
			
			// std::cout << "starting threads for downloading" << endl;
			vector<thread> get_chunk;
			for(auto peer: download_vector)
			{
				get_chunk.push_back(thread(peer_chunk_download,peer.first.first,peer.first.second,filename,destination_path,peer.second));
			}

			for(auto &a: get_chunk) if(a.joinable()) a.join();
			truncate((destination_path+"/"+filename).c_str(),filesize);
			// cout << "joined" << endl;
			// // get the bit vector
			//create new thread
			// cout << peer_server_port << filename<<endl;
			socket_to_chunks.erase({filename,groupname});
			std::cout << "Downloaded " << filename<< endl;
			
			downloaded_files[filename] = "C";


			response = "upload_file,"+current_login+","+filename+","+groupname+","+to_string(filesize)+","+serverip+":"+to_string(serverport);

			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				
				bzero(buffer,SOCKET_BUFF);
			}
			
			chunks = (filesize+CHUNK_SIZE-1)/CHUNK_SIZE;
			for(int i  = 0 ; i < chunks; i++)
				{
					chunk_map_per_file[filename].insert(i);
						
			}
			
			file_to_file_path[filename] = destination_path+"/"+filename;
			
			//decide the which packets from which
			//piece selection algo

			// get the packets

			}
			else{
				if(response == "1") std::cout << "group doesn't exist" <<endl;
				else if( response == "2") std::cout << "file doesn't exist in the group" <<endl;
				else if( response == "3") std::cout << "user not in the group" <<endl;
				else if( response == "4") std::cout << "no seeder up" <<endl;

			}


			
		}
		else if(client_message == "create_user"){
			string userid, password;
			cin>> userid >> password;
			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+userid+","+password;
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				std::cout << buffer << endl;
				bzero(buffer,SOCKET_BUFF);
			}
		}
		else if(client_message == "show_downloads"){
			if(downloaded_files.size() > 0){
				for(auto a : downloaded_files){
					cout << a.second << " " << a.first << endl;
				}
			}
			else
			{
				cout << "No files downloaded" <<endl;
			}
			
		}
		else if(client_message == "login"){
			

			string userid, password;
			cin>> userid >> password;
			if(current_login == userid){
				std::cout << "Already logined with the user" << endl;
				std::cout << endl;
				
				continue;
			}
			else if(current_login != ""){
				std::cout << "A user already login logout first" << 	endl;
				std::cout << endl;
				
				continue;	
			}

			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+userid+","+password+","+serverip+":"+to_string(serverport);
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response=buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			if(response == "0"){
				current_login= userid;
				std::cout << "Login with " << userid << endl;
			}
			else if(response == "1"){
				std::cout << "Wrong password"<<endl;
			}
			else if(response == "2"){
				std::cout << "User doesn't exist"<<endl;
			}

		}
		else if(client_message == "logout"){
			

		
			 if(current_login == ""){
				std::cout << "No user login" << 	endl;
				std::cout << endl;

				continue;	
			}
			

			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+current_login+","+serverip+":"+to_string(serverport);
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response=buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			current_login="";
			std::cout<<response<<endl;
		

			

		}
		else if(client_message == "create_group"){
			

			string groupname;
			cin>> groupname ;
			if(current_login == ""){
				std::cout << "Not log-inned" << 	endl;
				std::cout << endl;

				continue;	
			}

			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+groupname+","+current_login;
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response=buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			std::cout << response<<endl;

		}
		else if(client_message == "join_group"){
			

			string groupname;
			cin>> groupname ;
			if(current_login == ""){
				std::cout << "Not log-inned" << 	endl;
				std::cout << endl;

				continue;	
			}

			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+groupname+","+current_login;
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response=buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			std::cout << response<<endl;

		}
		else if(client_message == "leave_group"){
			

			string groupname;
			cin>> groupname ;
			if(current_login == ""){
				std::cout << "Not log-inned" << 	endl;
				std::cout << endl;

				continue;	
			}

			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+groupname+","+current_login;
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response=buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			std::cout << response<<endl;

		}
		else if(client_message == "requests"){
			cin>> client_message;
			if(client_message == "list_requests"){
			string groupname;
			cin>> groupname ;
			if(current_login == ""){
				std::cout << "Not log-inned" << 	endl;
				std::cout << endl;

				continue;	
			}

			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+groupname+","+current_login;
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response=buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			std::cout << response<<endl;
			}
			else{
				std::cout << "Wrong parameter with requests" << endl;
			}

		}
		else if(client_message == "accept_request"){
			
			string groupname,request_user_id;
			cin>> groupname >> request_user_id ;
			if(current_login == ""){
				std::cout << "Not log-inned" << 	endl;
				std::cout << endl;
				continue;	
			}

			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+","+groupname+","+current_login+","+request_user_id;
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response=buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			std::cout << response<<endl;
			

		}
		else if(client_message == "list_groups"){
			
			
			if(current_login == ""){
				std::cout << "Not log-inned" << 	endl;
				std::cout << endl;

				continue;	
			}

			char buffer[SOCKET_BUFF] = {0};
	        string response(SOCKET_BUFF,'\0');

			response = client_message+",";
			send(sock , response.c_str() , response.capacity() , 0 );
			if((valread = read( sock , buffer, SOCKET_BUFF)) > 0){
				response=buffer;
				bzero(buffer,SOCKET_BUFF);
			}
			std::cout << response<<endl;
			

		}
		else{
			std::cout << "wrong command" << endl;
		}
		std::cout << endl;
	}
	
	
	return 0; 
} 
