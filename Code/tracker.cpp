#include <unistd.h> 
#include <bits/stdc++.h>
#include <stdio.h> 
#include <arpa/inet.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <thread>
#define SOCKET_BUFF 16384
using namespace std;
struct fileinfo{
    int filesize;
    string hash;
    // string group; //this has to be commented
    set<pair<string,int>> sockets;

};
struct userinfo{
    string password;
    set<string> groups;
};
struct groupinfo{
    string owneruser;
    set<string> files;
    set<string> requests;

};
string tracker_ip;
int tracker_port;
map<string,userinfo> alluserinfo;
map<string,groupinfo> allgroupinfo;
map<pair<string,string>,fileinfo> allfileinfo; // filename,group
map<string,bool> peer_status;
vector<thread> thread_vector;
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

void serverfunc(int sd,int client_port){
    while(1){
	char  buffer[SOCKET_BUFF];
    string request;
    int  valread;
    if(( valread = read( sd , buffer, SOCKET_BUFF))>0) {
        request=buffer;
        cout << request << endl;
        bzero(buffer, SOCKET_BUFF);
    }
    
    
    vector<string> req_vec = split(request,',');
    if(req_vec[0] == "upload_file"){
        // upload_file userid filename  groupname size serverip:serverport
        
        /* 
        Conditions to be checked:
        group conditions:
        1. Exists
        2. User part of the group
        3. File doesn't exist
        */
       string response(SOCKET_BUFF,'\0');
       string username = req_vec[1];
       string filename = req_vec[2];
       string groupname = req_vec[3];
       string filesize = req_vec[4];
       string peer = req_vec[5];
       vector<string> peer_socket = split(req_vec[5],':');
       string peer_ip = peer_socket[0];
       int peer_port = atoi(peer_socket[1].c_str());
        response="";
        if(allgroupinfo.find(groupname) != allgroupinfo.end()){
            if(alluserinfo[username].groups.find(groupname)!= alluserinfo[username].groups.end() ){
                
                if(allfileinfo.find({filename,groupname}) != allfileinfo.end()){
                    // file exist
                    // check if same file by size 
                    if(to_string(allfileinfo[{filename,groupname}].filesize) == filesize){
                        if(allfileinfo[{filename,groupname}].sockets.find({peer_ip,peer_port}) == allfileinfo[{filename,groupname}].sockets.end()){
                            allfileinfo[{filename,groupname}].sockets.insert({peer_ip,peer_port});
                            response="User:"+username+" seeded "+filename+" of size "+filesize+"B to group "+groupname;

                        }
                        else{
                            //file already uploaded by peer
                            response="User:"+username+" already seeded "+filename+" of size "+filesize+"B to group "+groupname;

                        }
                    }
                    else{
                        // some other file with same name
                        response="The group " + groupname + " already contains a different file with the filename "+ filename;
                    }
        
                }
                else{
                    //file doesn't exist
                    fileinfo f;
                    f.filesize = atoi(filesize.c_str());
                    f.sockets.insert({peer_ip,peer_port});
                    allgroupinfo[groupname].files.insert(filename);
                    allfileinfo.insert({{filename,groupname}, f});
                    response="User:"+username+" uploaded "+filename+" of size "+filesize+"B to group "+groupname;
                }
                


            }
            else{
                response="User:"+username+" is not in the group "+groupname;
            }
            
            
        }
        else{
            response="Group doesn't exists";
        }
        // cout<<response<<endl;
        send(sd , response.c_str() , response.capacity() , 0 );


       
    }
    // download,peer1
    // else if(req_vec[0] == "download"){
    //     string response(SOCKET_BUFF,'\0');
    //     string filename = req_vec[1];
    //     int filesize = allfileinfo[filename].filesize;
    //     response = to_string(filesize);

    //     // inserting seeders
    //     for(auto a: files_to_port[filename]){
    //         response+=","+to_string(a);
    //     }
    //     cout << response << endl;
    //     send(sd , response.c_str() , response.capacity() , 0 );
    // }
    else if(req_vec[0] == "create_user"){
        // create_user username password
        string username = req_vec[1];
        string password = req_vec[2];
        string response(SOCKET_BUFF,'\0');
        response="";
        if(alluserinfo.find(username)==alluserinfo.end()){
            userinfo ui ;
            ui.password = password;
            alluserinfo[username] = ui;
            response="User "+username+" created";
        }
        else{
            response="User already exists";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "download_file"){
        // download_file username filename groupname
        string username = req_vec[1];
        string filename = req_vec[2];
        string groupname = req_vec[3];
        string response(SOCKET_BUFF,'\0');
        response="";

        /* 
        Conditions:
        1. groupexists
        2. file exists in the group
        3. user is in the group
        4. File has seeders
        5. Check if seeders are up or not
        6. Send filesize,hash,seeders list 
        */

        if(allgroupinfo.find(groupname) != allgroupinfo.end()){
            if(allgroupinfo[groupname].files.find(filename) != allgroupinfo[groupname].files.end()){
                if(alluserinfo[username].groups.find(groupname) != alluserinfo[username].groups.end()){
                    bool any_seeder_up = false;
                    for(auto a:allfileinfo[{filename,groupname}].sockets){
                        string peer_socket = a.first +":"+to_string(a.second);
                        if(peer_status[peer_socket]){
                            any_seeder_up = true;
                            break;
                        }
                    }
                    if(any_seeder_up){
                        response="0";
                    }
                    else{
                        // no seeder up
                        response="4";
                    }
                }
                else{
                    //user not in the group;
                    response = "3";
                }
            }
            else{
                //file doesn't exist in the group
                response = "2";
            }
        }
        else{
            // group doesn't exist
            response = "1";
        }
        
        send(sd , response.c_str() , response.capacity() , 0 );
        if(response == "0"){
            string response(SOCKET_BUFF,'\0');
            response="";
            response+=to_string( allfileinfo[{filename,groupname}].filesize);
            for(auto a: allfileinfo[{filename,groupname}].sockets){
                string peer_socket = a.first +":"+to_string(a.second);
                if(peer_status[peer_socket]){
                    response+=","+peer_socket;
                }
            }
            cout << response << endl;
            send(sd , response.c_str() , response.capacity() , 0 );

        }

    
    
    }
    else if(req_vec[0] == "login"){
        // login username password serverip:serverport
        string username = req_vec[1];
        string password = req_vec[2];
        string peer_scoket = req_vec[3];
        string response(SOCKET_BUFF,'\0');
        response="";
        // 0 => correct authentication
        // 1 => invalid password
        // 2 => user doesn't exist
        if(alluserinfo.find(username)!=alluserinfo.end()){
            if(alluserinfo[username].password == password)
                {
                    response="0";
                    peer_status[peer_scoket] = 1;
                }
            else
                response="1";
        }
        else{
            response="2";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "logout"){
        // login username serverip:serverport
        string username = req_vec[1];
        // string password = req_vec[2];
        string peer_scoket = req_vec[2];
        string response(SOCKET_BUFF,'\0');
        response="";
       
        if(peer_status[peer_scoket]){
            peer_status[peer_scoket] = false;
            response="Logged out";
        }
        else{
            response="Not logged in";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "stop_share"){
        // stop_share username filename groupname serverip:serverport
        string username = req_vec[1];
        string filename = req_vec[2];
        string groupname = req_vec[3];
        string peer_scoket = req_vec[4];
        vector<string> peer_scoket_vector = split(peer_scoket,':');
        string peer_ip = peer_scoket_vector[0];
        int peer_port = atoi(peer_scoket_vector[1].c_str());
        /* 
        Conditions:
        1. groupexists
        2. is a member of group
        3. file exists in the group
        4. file has socket of serverip:serverport
        5. if only socket in that file
         */
        string response(SOCKET_BUFF,'\0');
        response="";
       
        if(allgroupinfo.find(groupname)!= allgroupinfo.end()){
            if(alluserinfo[username].groups.find(groupname) != alluserinfo[username].groups.end()){
                if(allgroupinfo[groupname].files.find(filename) != allgroupinfo[groupname].files.end()){
                    if(allfileinfo[{filename,groupname}].sockets.find({peer_ip,peer_port}) != allfileinfo[{filename,groupname}].sockets.end()){
                        response = "Stopped sharing";
                        if(allfileinfo[{filename,groupname}].sockets.size() > 1){
                            allfileinfo[{filename,groupname}].sockets.erase({peer_ip,peer_port});
                        }
                        else{
                            allfileinfo.erase({filename,groupname});
                            allgroupinfo[groupname].files.erase(filename);

                        }
                    }
                    else{
                        response="File: "+ filename+" has not been shared on "+peer_scoket;
                    }
                }
                else{
                    response="No such file "+filename+" in the group "+groupname;
                }
            }
            else{
                response="User:"+username+" is not in the group "+groupname;
            }
        }
        else{
            response="Group "+ groupname +" doesn't exist";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "create_group"){
        // create_user groupname userid
        string groupname = req_vec[1];
        string userid = req_vec[2];
        string response(SOCKET_BUFF,'\0');
        response="";
        if(allgroupinfo.find(groupname) == allgroupinfo.end()){
            groupinfo gi;
            gi.owneruser= userid;
            allgroupinfo[groupname] = gi;
            alluserinfo[userid].groups.insert(groupname);
            
            response="Group "+groupname+" created";
        }
        else{
            response="Group already exists";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "join_group"){
        // join_group groupname userid
        string groupname = req_vec[1];
        string userid = req_vec[2];
        string response(SOCKET_BUFF,'\0');
        response="";
        if(allgroupinfo.find(groupname) != allgroupinfo.end()){
            if(alluserinfo[userid].groups.find(groupname)== alluserinfo[userid].groups.end() ){
                if(allgroupinfo[groupname].owneruser == userid){
                    alluserinfo[userid].groups.insert(groupname);
                    response="User:"+userid+"is the owner of "+groupname+" adding directly";
                }
                else{
                    if(allgroupinfo[groupname].requests.find(userid) == allgroupinfo[groupname].requests.end())
                    {
                        allgroupinfo[groupname].requests.insert(userid);
                        response="User:"+userid+" requested owner of "+groupname+" to add in group.";
                    }
                    else{
                        response="User:"+userid+" has already requested owner of "+groupname+" to add in group.";
                        
                    }
                }
            }
            else{
                response="User:"+userid+" already in the group "+groupname;
            }
            
            
        }
        else{
            response="Group doesn't exists";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "leave_group"){
        // leave_group groupname userid
        string groupname = req_vec[1];
        string userid = req_vec[2];
        string response(SOCKET_BUFF,'\0');
        response="";
        if(allgroupinfo.find(groupname) != allgroupinfo.end()){
            if(alluserinfo[userid].groups.find(groupname)!= alluserinfo[userid].groups.end() ){
                alluserinfo[userid].groups.erase(groupname);
                response="User:"+userid+" left the group "+groupname;
            }
            else{
                response="User:"+userid+" not in the group "+groupname;
            }
            
            
        }
        else{
            response="Group doesn't exists";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "list_requests"){
        // list_requests groupname userid
        string groupname = req_vec[1];
        string userid = req_vec[2];
        string response(SOCKET_BUFF,'\0');
        response="";
        if(allgroupinfo.find(groupname) != allgroupinfo.end()){
                    if(allgroupinfo[groupname].owneruser == userid)
                    {
                        if(allgroupinfo[groupname].requests.size()>0){
                            int i = 1;
                            response = "";
                            for(auto a: allgroupinfo[groupname].requests){
                                response+=to_string(i)+ " "+a+"\n";
                                i++;
                            }
                        }
                        else{
                            response = "No requests";
                        }
                    }
                    else{
                        response="User:"+userid+" is not the owner of the group "+groupname;
                        
                    }
                
           
            
            
        }
        else{
            response="Group doesn't exists";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "accept_request"){
        // accept_request groupname currentid reqid
        string groupname = req_vec[1];
        string currentid = req_vec[2];
        string requid = req_vec[3];
        string response(SOCKET_BUFF,'\0');
        response="";
        if(allgroupinfo.find(groupname) != allgroupinfo.end()){
                    if(allgroupinfo[groupname].owneruser == currentid)
                    {
                        if(allgroupinfo[groupname].requests.find(requid)!=allgroupinfo[groupname].requests.end()){
                            allgroupinfo[groupname].requests.erase(requid);
                            alluserinfo[requid].groups.insert(groupname);

                            response="User:"+requid+" was added to "+groupname+" group.";
                        }
                        else{
                            response = "No request was made by "+requid + " for group "+groupname;
                        }
                    }
                    else{
                        response="User:"+currentid+" is not the owner of the group "+groupname;
                        
                    }
                
           
            
            
        }
        
        else{
            response="Group doesn't exists";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "list_groups"){
        // list_groups
       
        string response(SOCKET_BUFF,'\0');
        response="";
        if(allgroupinfo.size()>0){
            int i = 1;
            response = "";
            for(auto a: allgroupinfo){
                response+=to_string(i)+ " "+a.first+"\n";
                i++;
            }
        }
        else{
            response="No groups";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else if(req_vec[0] == "list_files"){
        // list_files userid groupname
        /* 
        Check:
        1. groupexists
        2. user part of group
        3. if files in the group

         */
        string userid = req_vec[1];
        string groupname = req_vec[2];
        string response(SOCKET_BUFF,'\0');
        response="";
        if(allgroupinfo.find(groupname) != allgroupinfo.end()){
            if(alluserinfo[userid].groups.find(groupname)!= alluserinfo[userid].groups.end() ){
                if(allgroupinfo[groupname].files.size()>0)
                {int i = 1;
                response = "";
                for(auto a: allgroupinfo[groupname].files){
                    response+=to_string(i)+ " "+a+"\n";
                    i++;
                }
                }
                else{
                    response="No files";
                }
            }
            else{
                response="User:"+userid+" doesn't belong to the group "+groupname;
            }
        }
        else{
            response="No such group exists";
        }
        send(sd , response.c_str() , response.capacity() , 0 );

    }
    else{
        string response(SOCKET_BUFF,'\0');
        response = "Not a valid command";
        send(sd , response.c_str() , response.capacity() , 0 );
    }
    
    }
}


int main(int argc, char const *argv[]) 
{ 
	
    string tracker_info = argv[1];
    fstream in(tracker_info,ios::in);
    string tracker;
    getline(in, tracker,'\n');
    vector<string> trackersocket = split(tracker,':');
    tracker_ip = trackersocket[0];
    tracker_port = atoi(trackersocket[1].c_str());
    int opt = 1; 


	int server_fd, new_socket, valread; 
	struct sockaddr_in address; 
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
    address.sin_addr.s_addr = inet_addr(tracker_ip.c_str());
	// address.sin_addr.s_addr = INADDR_ANY; 
	address.sin_port = htons( tracker_port ); 
	
	if (bind(server_fd, (struct sockaddr *)&address,sizeof(address))<0) 
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
    while((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen))){
        i++;
		if (new_socket<0) { 
				perror("accept"); 
				exit(EXIT_FAILURE); 
		}
		else{
            int client_port = ntohs( address.sin_port);
            cout <<"Request for "<< client_port << endl;
			thread_vector.push_back( thread(serverfunc,new_socket,client_port));
			
		}
    
    }
    for(auto &th : thread_vector){
        if(th.joinable())   
            th.join();
    }
	return 0; 
} 
