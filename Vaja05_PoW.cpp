// Vaja05_PoW.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <shared_mutex>
#include <mutex>
#include "TaskQueue.h"
#include "sha256.h"

using namespace std;

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

struct block {
	int index;
	string data;
	string timestamp;
	string previoushash;
	int nonce;
	string hash;
};

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

void printLastBlock(vector<block*> OfficialBlocks) {
	cout << "=====================================================================================\n";
	cout << "Index: " << OfficialBlocks[OfficialBlocks.size() - 1]->index << endl;
	cout << "Data: " << OfficialBlocks[OfficialBlocks.size() - 1]->data << endl;
	cout << "Timestamp: " << OfficialBlocks[OfficialBlocks.size() - 1]->timestamp << endl;
	cout << "PreviousHash: " << OfficialBlocks[OfficialBlocks.size() - 1]->previoushash << endl;
	cout << "Nonce: " << OfficialBlocks[OfficialBlocks.size() - 1]->nonce << endl;
	cout << "Hash: " << OfficialBlocks[OfficialBlocks.size() - 1]->hash << endl;
	cout << endl;
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

int main()
{
	rngEngine* rng = new rngEngine(2);	//rng engine, vsak mpi node bo dobil svoj seed
	vector<block*> OfficialBlocks;	//najdeni bloki
	//int difficulty = 20;
	int threadsLmt = thread::hardware_concurrency();
	//threadsLmt = 1;

	shared_mutex smtx;
	auto progstart = std::chrono::high_resolution_clock::now();	//čas izvajanja programa
	int hasheschechked = 0;	//število predelanih hashov na tem vozlišču
	for (int r = 0; r < 10; r++) {
		block* tmp = new block();
		fillData(tmp, OfficialBlocks);

		TaskQueue* tpp = new TaskQueue(threadsLmt);	//Ustvarim threade
		for (int j = 0; j < threadsLmt; j++) {	//vsak thread dobi nalogo da rudari blok
			tpp->AddJob([&] {
				//cout << "Computing...\n";
				smtx.lock_shared();
				int tIndex = tmp->index;
				string tData = tmp->data;
				string tTimestamp = tmp->timestamp;
				string tPrevioushash = tmp->previoushash;
				smtx.unlock_shared();

				int calcNonce = 0;
				// MineBlock
				while (true) {
					calcNonce = rng->generateNumInt();	//generiram nonce
					//Pridobim sha256 hash
					string src_str = to_string(tIndex) + tData + tTimestamp + tPrevioushash + to_string(calcNonce);

					smtx.lock_shared();
					string hash_hex_str = sha256(src_str); //pridobim hash
					smtx.unlock_shared();

					string ref = hash_hex_str.substr(0, 5); //nastavim težavnost s tem da zahtevam, da se hash začne z ničlami

					if (ref == "00000") {	//ko najde primeren hash
						smtx.lock_shared();
						tmp->nonce = calcNonce;
						tmp->hash = hash_hex_str;
						OfficialBlocks.push_back(tmp);
						smtx.unlock_shared();
						break;
					}

					if (tpp->checkThreadCloseSignal()) {	//vsi ostali threadi so seznanjeni da je bil hash za ta blok že najden
						break;
					}
					smtx.lock_shared();
					hasheschechked++;
					smtx.unlock_shared();
				}
				});
		}
		tpp->JoinAllInterrupt();

		printLastBlock(OfficialBlocks);
		delete tpp;
	}

	//printBlockChain(OfficialBlocks);

	auto progfinish = std::chrono::high_resolution_clock::now();
	double progtime = std::chrono::duration_cast<std::chrono::nanoseconds>(progfinish - progstart).count();
	double ptime = progtime / 1000000000.0;

	cout << "Time requred =" << ptime << "s" << endl;
	cout << "HashesChecked =" << hasheschechked << endl;
	cout << "Hashes per sec = " << hasheschechked / ptime << endl << endl;

	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

