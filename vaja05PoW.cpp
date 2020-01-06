#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <sstream>
#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <shared_mutex>
#include <mutex>
#include "TaskQueue.h"
#include "sha256.h"
#include <mpi.h>

using namespace std;

int stop_signal = 0;

struct block {
	int index;
	string data;
	string timestamp;
	string previoushash;
	int nonce;
	string hash;
};

vector<block*> OfficialBlocks;

class rngEngine {
private:
	random_device rd;
	mt19937 seed;
public:
	rngEngine() : seed(rd()) {
	};
	rngEngine(int num) : seed(num) {
	};
	double generateNum() {
		uniform_real_distribution<> distribution(0.0, 1.0);
		return distribution(seed);
	};
	int generateNumInt(int min, int max) {
		uniform_int_distribution<> distribution(min, max);
		return distribution(seed);
	}
	int generateNumInt() {
		uniform_int_distribution<> distribution(0, INT32_MAX);
		return distribution(seed);
	}
};

string hextobin(string ss)
{
	string tt = ""; //string sestavljen iz bin stevil
	for (int i = 0; i < ss.length(); i++)
	{
		switch (toupper(ss[i]))
		{
		case '0': tt += "0000"; break;
		case '1': tt += "0001"; break;
		case '2': tt += "0010"; break;
		case '3': tt += "0011"; break;
		case '4': tt += "0100"; break;
		case '5': tt += "0101"; break;
		case '6': tt += "0110"; break;
		case '7': tt += "0111"; break;
		case '8': tt += "1000"; break;
		case '9': tt += "1001"; break;
		case 'A': tt += "1010"; break;
		case 'B': tt += "1011"; break;
		case 'C': tt += "1100"; break;
		case 'D': tt += "1101"; break;
		case 'E': tt += "1110"; break;
		case 'F': tt += "1111"; break;
		}
	}
	return tt;
}

void fillData(block* tmp, vector<block*> OfficialBlock) {
	tmp->index = OfficialBlock.size();
	tmp->data = "notImportant";

	//get timestamp
	auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
	tmp->timestamp = ctime(&timenow);
	tmp->timestamp = tmp->timestamp.substr(0, tmp->timestamp.length() - 1);

	//get previoushash
	if (OfficialBlock.size() == 0) {
		tmp->previoushash = "0";
	}
	else {
		tmp->previoushash = OfficialBlock[OfficialBlock.size() - 1]->hash;
	}
}

void printLastBlock() {
	ostringstream os;
    	os << "============================================================================\n"<< "Index: " << OfficialBlocks[OfficialBlocks.size() - 1]->index << endl << "Data: " << OfficialBlocks[OfficialBlocks.size() - 1]->data << endl << "Timestamp: " << OfficialBlocks[OfficialBlocks.size() - 1]->timestamp << endl << "PreviousHash: " << OfficialBlocks[OfficialBlocks.size() - 1]->previoushash << endl << "Nonce: " << OfficialBlocks[OfficialBlocks.size() - 1]->nonce << endl << "Hash: " << OfficialBlocks[OfficialBlocks.size() - 1]->hash << endl << endl;
    	string s = os.str();
    	cout << s << endl;
}

void printBlockChain(vector<block*> OfficialBlocks) {
	for (int i = 0; i < OfficialBlocks.size(); i++) {
		cout << "=====================================================================================\n";
		cout << "Index: " << OfficialBlocks[i]->index << endl;
		cout << "Data: " << OfficialBlocks[i]->data << endl;
		cout << "Timestamp: " << OfficialBlocks[i]->timestamp << endl;
		cout << "PreviousHash: " << OfficialBlocks[i]->previoushash << endl;
		cout << "Nonce: " << OfficialBlocks[i]->nonce << endl;
		cout << "Hash: " << OfficialBlocks[i]->hash << endl;
		cout << endl;
	}
}

void listen(){
	int rank, size,r;
	r = 1;
	
	MPI_Status status;
    MPI_Request request;

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	char* recvHash = new char[64];	//master sem prejme hash od vozlisca ki je najprej koncalo

	if(rank == 0){
		MPI_Recv(recvHash, 64, MPI_CHAR,MPI_ANY_SOURCE , 0, MPI_COMM_WORLD, &status );	//prejme hash od 1.
		string str(recvHash);

		OfficialBlocks[OfficialBlocks.size() - 1]->hash = str.substr(0,64);	//pridobljen hash vstavi v string

		stop_signal = 1;	//ustavi iskanje hasha na masterju
		for(int i=1;i<size;i++){	//master obvesti ostala vozlisca naj nehajo iskat hash saj je bil ze najden
			MPI_Isend(&r,1,MPI_INT,i,0,MPI_COMM_WORLD,&request);
		}
		
		printLastBlock();

	}else{
		MPI_Recv( &stop_signal, 1, MPI_INT,0 , 0, MPI_COMM_WORLD, &status );	//ostala vozlisca cakajo da jih master obvesti naj nehajo z iskanjem
	}

}

int main(int argc, char* argv[])
{
	int rank, size;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	MPI_Status status;
    MPI_Request request=MPI_REQUEST_NULL;

	int seed = rank; //vsaka noda dobi svoj seed, zato da uporabljajo razlicne nonce	

	rngEngine* rng = new rngEngine(seed);	//rng engine, vsak mpi node dobi svoj seed
	
	int threadsLmt = thread::hardware_concurrency(); //pridobim st. jeder procesorja
	string blockData;

	shared_mutex smtx;

	auto progstart = std::chrono::high_resolution_clock::now();	//čas izvajanja programa
	int hasheschechked = 0;	//število predelanih hashov na tem vozlišču


	for (int r = 0; r < 3; r++) {
		stop_signal = 0;	
		block* tmp = new block();

		//MASTER
		if (rank == 0) {
			fillData(tmp, OfficialBlocks);	//block napolnim s podatki
			OfficialBlocks.push_back(tmp);

			blockData = to_string(tmp->index) + tmp->data + tmp->timestamp + tmp->previoushash;	//podatke dam v string da jih razposljem, nad tem bojo ostali iskali hash
			int hashSize = blockData.length();

			for (int i = 1; i < size; i++) {
				MPI_Send(&hashSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD);	//najprej posljem velikost sporocila ostalim vozliscem
			}
			for (int i = 1; i < size; i++) {
				MPI_Send(blockData.c_str(), hashSize, MPI_CHAR, i, 0, MPI_COMM_WORLD);	//nato poslem se sporocilo
				//cout << "POSLANO" << endl;
			}
		}

		//SLAVE
		if (rank != 0) {
			int hashSize;
			MPI_Recv(&hashSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	//ostala vozlisca sprejmejo velikost sporocila
			char* data = new char[hashSize];
			MPI_Recv(data, hashSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	//in sporocilo

			blockData = data;
		}

		std::thread listener(listen);	//nastavim listener

		TaskQueue* tpp = new TaskQueue(threadsLmt);	//Ustvarim threade
		for (int j = 0; j < threadsLmt; j++) {	//vsak thread dobi nalogo da rudari blok
			tpp->AddJob([&] {
				int calcNonce = 0;

				// MineBlock
				while (true) {
					calcNonce = rng->generateNumInt();	//generiram nonce

					//Pridobim sha256 hash - rudarim
					string src_str = "";
					src_str = blockData + to_string(calcNonce);	

					smtx.lock_shared();
					string hash_hex_str = sha256(src_str); //pridobim hash
					smtx.unlock_shared();

					string ref = hash_hex_str.substr(0, 5); //nastavim težavnost s tem da zahtevam, da se hash začne z ničlami

					if (ref == "00000") {	//ko najde primeren hash ga posljem masterju kjer ga recv listener
						MPI_Send(hash_hex_str.c_str(), 64, MPI_CHAR, 0, 0, MPI_COMM_WORLD);			
						break;
					}

					smtx.lock_shared();
					hasheschechked++;
					smtx.unlock_shared();

					if (tpp->checkThreadCloseSignal()) {	//vsi ostali threadi so seznanjeni da je bil hash za ta blok že najden
						break;
					}
					if(stop_signal == 1){	//ce je dobilo vozlisce stop signal od masterja na listenerju neha z iskanjem hasha
						stop_signal =0;
						break;
					}
				}
			});
		}
		tpp->JoinAllInterrupt();
		
		delete tpp;
		listener.join();

		MPI_Barrier( MPI_COMM_WORLD );
	}

	auto progfinish = std::chrono::high_resolution_clock::now();
	double progtime = std::chrono::duration_cast<std::chrono::nanoseconds>(progfinish - progstart).count();
	double ptime = progtime / 1000000000.0;

	//pridobim podatke o stevilu pregledanih hashov posameznih vozlisc

	vector<int> workInfo(size);
	workInfo[0] = hasheschechked; //za masterja lahko kar takoj zapisem st. hashov
	int allHashes = hasheschechked;
	int tmpHs;	//sem master sprejme podatke o pregledanih hashih na drugih vozliscih
	if(rank != 0){
		MPI_Send(&hasheschechked, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);	//ostala vozlisca posljejo svoje podatke masterju
	}else{
		for(int t=0;t<size-1;t++){
			MPI_Recv(&tmpHs, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);	//master prejme podatke in jih obdela
			workInfo[status.MPI_SOURCE] = tmpHs;
			allHashes+= tmpHs;
		}
	}

	if( rank == 0){	//izpis delovanja programa
		for(int i = 0; i<size; i++){
			cout << "Node: " << i << "\tHasheschecked: " << workInfo[i] << endl;
		}
		cout << "Time requred =" << ptime << "s" << endl;
		cout << "HashesChecked =" << allHashes << endl;
		cout << "Hashes per sec = " << allHashes / ptime << endl << endl;
	
	}
	
	MPI_Finalize();
	return 0;

}
