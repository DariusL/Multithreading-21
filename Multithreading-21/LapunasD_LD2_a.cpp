#include <mutex>
#include <condition_variable>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <fstream>
#include <thread>
#include <future>

using namespace std;

class Semaphore
{
private:
	mutex mtx;
	condition_variable condition;
    int count;

public:
	Semaphore():count(1){}
    void Notify()
    {
		unique_lock<mutex> lock(mtx);
		count++;
        condition.notify_one();
    }
    void Wait()
    {
		unique_lock<mutex> lock(mtx);
		condition.wait(lock, [=]{return count > 0;});
		count--;
    }
};

struct Struct
{
	string pav;
	int kiekis;
	double kaina;
	Struct(string input);
	string Print();
};

Struct::Struct(string input)
{
	int start, end;
	start = 0;
	end = input.find(' ');
	pav = input.substr(0, end);
	start = end + 1;
	end = input.find(' ', start);
	kiekis = stoi(input.substr(start, end - start));
	start = end + 1;
	kaina = stod(input.substr(start));
}

string Struct::Print()
{
	stringstream ss;
	ss << setw(15) << pav << setw(7) << kiekis << setw(20) << kaina;
	return ss.str();
}

struct Counter
{
	string pav;
	int count;
public:
	Counter(string pav, int count):pav(pav), count(count){}
	Counter(string line);
	int operator++(){return ++count;}
	int operator--(){return --count;}
	bool operator==(const Counter &other){return pav == other.pav;}
	bool operator<(const Counter &other){return pav < other.pav;}
};

Counter::Counter(string line)
{
	int start, end;
	start = 0;
	end = line.find(' ');
	pav = line.substr(0, end);
	start = end + 1;
	end = line.find(' ', start);
	count = stoi(line.substr(start, end - start));
}

class Buffer
{
	vector<Counter> buffer;
	Semaphore semaphore;
public:
	Buffer(){}
	void Add(string pav);
	bool Take(string pav);
	int Size();
};

void Buffer::Add(string pav)
{
	semaphore.Wait();
	Counter c(pav, 1);
	auto i = find(buffer.begin(), buffer.end(), c);
	if(i != buffer.end())
	{
		++(*i);
	}
	else
	{
		auto size = buffer.size();
		for(auto i = buffer.begin(); i != buffer.end(); i++)
		{
			if(c < (*i))
			{
				buffer.insert(i, c);
				break;
			}
		}
		if(buffer.size() == size)
			buffer.push_back(c);
	}
	semaphore.Notify();
}

bool Buffer::Take(string pav)
{
	semaphore.Wait();
	Counter c(pav, 0);
	auto i = find(buffer.begin(), buffer.end(), c);
	if(i != buffer.end())
	{
		--(*i);

		if((*i).count <= 0)
			buffer.erase(i);

		semaphore.Notify();
		return true;
	}
	else
	{
		semaphore.Notify();
		return false;
	}
}

int Buffer::Size()
{
	semaphore.Wait();
	int size = buffer.size();
	semaphore.Notify();
	return size;
}

string Titles();
string Print(int nr, Struct &s);
void syncOut(vector<vector<Struct>>&);

vector<vector<Struct>> ReadStuff(string file);
vector<vector<Counter>> ReadCounters(string file);
vector<string> ReadLines(string file);
void Make(vector<Struct> stuff);
vector<Counter> Use(vector<Counter> stuff);

Buffer buffer;
volatile bool done = false;

int main()
{
	auto input = ReadStuff("LapunasD_L2.txt");
	auto userStuff = ReadCounters("LapunasD_L2.txt");

	cout << Titles() << endl;
	syncOut(input);

	vector<thread> makers;
	vector<future<vector<Counter>>> users;

	for(auto &v : input)
		makers.emplace_back(Make, v);

	for(auto &v : userStuff)
		users.push_back(async(Use, v));

	for(auto &t : makers)
		t.join();
	done = true;

	for(auto &f : users)
	{
		f.wait();
		auto &res = f.get();
		for(auto &c : res)
			cout << c.pav << " " << c.count << endl;
		cout << endl;
	}



	system("pause");
	return 0;
}

vector<vector<Struct>> ReadStuff(string file)
{
	auto lines = ReadLines(file);
	vector<vector<Struct>> ret;
	vector<Struct> tmp;
	for(unsigned int i = 0; i < lines.size(); i++)
	{
		if(lines[i] == "vartotojai")
		{
			break;
		}
		if(lines[i] == "")
		{
			ret.push_back(move(tmp));
		}
		else
		{
			tmp.emplace_back(lines[i]);
		}
	}
	return ret;
}

vector<string> ReadLines(string file)
{
	vector<string> ret;
	ifstream duom(file);
	while(!duom.eof())
	{
		string line;
		getline(duom, line);
		ret.push_back(line);
	}
	return ret;
}

vector<vector<Counter>> ReadCounters(string file)
{
	auto lines = ReadLines(file);
	vector<vector<Counter>> ret;
	vector<Counter> tmp;
	int i;
	for(i = 0; i < lines.size(); i++)
	{
		if(lines[i] == "vartotojai")
			break;
	}
	for(i++; i < lines.size(); i++)
	{
		if(lines[i] == "")
			ret.push_back(move(tmp));
		else
			tmp.emplace_back(lines[i]);
	}
	return ret;
}

string Titles()
{
	stringstream ss;
	ss << setw(15) << "Pavadiniams" << setw(7) << "Kiekis" << setw(20) << "Kaina";
	return ss.str();
}

void syncOut(vector<vector<Struct>> &data)
{
	cout << setw(3) << "Nr" << Titles() << endl << endl;
	for(unsigned int i = 0; i < data.size(); i++)
	{
		auto vec = data[i];
		cout << "Procesas_" << i << endl;
		for(unsigned int j = 0; j < vec.size(); j++)
		{
			cout << Print(j, vec[j]) << endl;
		}
	}
}

string Print(int nr, Struct &s)
{
	stringstream ss;
	ss << setw(3) << nr << s.Print();
	return ss.str();
}

void Make(vector<Struct> stuff)
{
	for(auto &s : stuff)
		buffer.Add(s.pav);
}

vector<Counter> Use(vector<Counter> stuff)
{
	auto i = stuff.begin();
	while((!done || buffer.Size() > 0) && stuff.size() > 0)
	{
		if(buffer.Take((*i).pav))
			--(*i);

		else if(done)
		{
			stuff.erase(i);
			i = stuff.begin();
			continue;
		}
		if((*i).count <= 0)
		{
			stuff.erase(i);
			i = stuff.begin();
			continue;
		}

		i++;
		if(i == stuff.end())
			i = stuff.begin();

	}
	return stuff;
}