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

volatile bool doneMaking = false;
volatile bool doneUsing = false;

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
	condition_variable accessCondition;
	condition_variable overflowCondition;
	bool accessing;
	volatile int count;
	static const int MAX = 200;
	mutex mtx;
public:
	Buffer() : accessing(false), count(0){}
	bool Add(Counter c);
	int Take(Counter c);
	int Size();
	string Print();
	void Done();
	int GetCount(){return count;}
private:
	void LockAccess();
	void UnlockAcces();
};

void Buffer::Done()
{
	overflowCondition.notify_all();
}

void Buffer::LockAccess()
{
	unique_lock<mutex> lock(mtx);
	accessCondition.wait(lock, [=]{return !accessing;});
	accessing = true;
}

void Buffer::UnlockAcces()
{
	unique_lock<mutex> lock(mtx);
	accessing = false;
	accessCondition.notify_one();
}

bool Buffer::Add(Counter c)
{
	unique_lock<mutex> lock(mtx);
	overflowCondition.wait(lock, [=]{return GetCount() < MAX || doneUsing;});
	lock.unlock();
	if(doneUsing)
		return false;
	LockAccess();
	count += c.count;
	auto i = find(buffer.begin(), buffer.end(), c);
	if(i != buffer.end())
	{
		(*i).count += c.count;
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
		overflowCondition.notify_one();
	}
	UnlockAcces();
	return true;
}

int Buffer::Take(Counter c)
{
	LockAccess();
	auto i = find(buffer.begin(), buffer.end(), c);
	int taken = 0;
	if(i != buffer.end())
	{
		if((*i).count >= c.count)
			taken = c.count;
		else
			taken = (*i).count;
		(*i).count -= taken;

		if((*i).count <= 0)
			buffer.erase(i);
	}
	count -= taken;
	UnlockAcces();
	overflowCondition.notify_one();
	return taken;
}

int Buffer::Size()
{
	LockAccess();
	int size = buffer.size();
	UnlockAcces();
	return size;
}

string Buffer::Print()
{
	stringstream ss;
	for(auto &c : buffer)
		ss << c.pav << " " << c.count << endl;
	return ss.str();
}

string Titles();
string Print(int nr, Struct &s);
void syncOut(vector<vector<Struct>>&);
void syncOut(vector<vector<Counter>>&);

vector<vector<Struct>> ReadStuff(string file);
vector<vector<Counter>> ReadCounters(string file);
vector<string> ReadLines(string file);
void Make(vector<Struct> stuff);
vector<Counter> Use(vector<Counter> stuff);
void MakeObserver(vector<thread> &makers);
void UseObserver(vector<future<vector<Counter>>> &users);

Buffer buffer;

int main()
{
	auto input = ReadStuff("LapunasD_L2.txt");
	auto userStuff = ReadCounters("LapunasD_L2.txt");

	cout << "\nGamintojai\n\n";
	syncOut(input);
	cout << "\nVartotojai\n\n";
	syncOut(userStuff);

	vector<thread> makers;
	vector<future<vector<Counter>>> users;
	vector<thread> observers;

	for(auto &v : input)
		makers.emplace_back(Make, v);

	for(auto &v : userStuff)
		users.push_back(async(Use, v));

	observers.emplace_back(MakeObserver, ref(makers));
	observers.emplace_back(UseObserver, ref(users));

	for(auto &o : observers)
		o.join();

	cout << "Vartotojam truko:\n\n";

	for(auto &f : users)
	{
		auto &res = f.get();
		for(auto &c : res)
			cout << c.pav << " " << c.count << endl;
		cout << endl;
	}

	cout << "Nesuvartota liko:\n\n" << buffer.Print();

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
	unsigned int i;
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
		auto &vec = data[i];
		cout << "Procesas_" << i << endl;
		for(unsigned int j = 0; j < vec.size(); j++)
		{
			cout << Print(j, vec[j]) << endl;
		}
	}
}

void syncOut(vector<vector<Counter>> &data)
{
	for(unsigned int i = 0; i < data.size(); i++)
	{
		auto &vec = data[i];
		cout << "Vartotojas_" << i << endl;
		for(unsigned int j = 0; j < vec.size(); j++)
		{
			cout << setw(15) << vec[j].pav << setw(5) << vec[j].count << endl;
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
		buffer.Add(Counter(s.pav, s.kiekis));
}

vector<Counter> Use(vector<Counter> stuff)
{
	auto i = stuff.begin();
	vector<Counter> ret;
	while((!doneMaking || buffer.Size() > 0) && stuff.size() > 0)
	{
		i++;
		if(i == stuff.end())
			i = stuff.begin();

		int taken = buffer.Take(*i);
		(*i).count -= taken;

		if(taken == 0 && doneMaking)
			ret.push_back(*i);

		if((*i).count <= 0 || (taken == 0 && doneMaking))
		{
			stuff.erase(i);
			i = stuff.begin();
		}
	}
	return ret;
}

void MakeObserver(vector<thread> &makers)
{
	for(auto &t : makers)
		t.join();
	doneMaking = true;
}

void UseObserver(vector<future<vector<Counter>>> &users)
{
	for(auto &f : users)
		f.wait();
	buffer.Done();
	doneUsing = true;
}