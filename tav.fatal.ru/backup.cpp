#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/utime.h>
#include <string>
#include <direct.h>
#include <time.h>

using namespace std;

struct stat *fexist(const string &filename,bool dir=false)
{
	static struct stat buffer;
	return (stat(filename.c_str(), &buffer)==0 && bool(buffer.st_mode & _S_IFDIR)==dir) ? &buffer : NULL;
}

void removedir(const string &dir)
{
	struct _finddata_t c_file;
	long hFile;

	if ((hFile=_findfirst((dir+"\\*.*").c_str(), &c_file))!=-1L)
	{
		do
		{
			string fullname=dir+"\\"+c_file.name;

			if (c_file.attrib & _A_SUBDIR)
			{
				if (strcmp(c_file.name,".") && strcmp(c_file.name,".."))
					removedir(fullname);
			}
			else
				remove(fullname.c_str());
		} while(_findnext(hFile, &c_file) == 0);

		_findclose(hFile);
	}

	rmdir(dir.c_str());
}

void makepath(const char *path)
{
	if (*path)
	{
		const char *p=path;

		while (true)
		{
			p=strchr(p,'\\');
			char dir[1000];
			int len=p ? (p-path) : strlen(path);
			strncpy(dir,path,len);
			dir[len]=0;
			if (dir[len-1]!=':'/*пропускаем имя диска*/ && !fexist(dir,true)) mkdir(dir);
			if (!p) break;
			p++;
		}
	}
}

void ToLog(const char *mes)
{
	FILE *flog=fopen("backup.log","at");
	fputs(mes,flog);
	fclose(flog);
}

string srcdir,backupdir;
char backuppostfix[100];
time_t curtime;

#define BLOCKSIZE (1024*1024/2)
char buf[BLOCKSIZE];

void parse(const string &dir,time_t dir_modified)
{
	struct _finddata_t c_file;
	long hFile;

	//Сравниваем дату изменения директории в источнике и в резервной копии
	struct stat *st=fexist(backupdir+dir,true);
	bool needForUpdateFiles=(st && st->st_mtime>=dir_modified) ? false : true;//Флаг нужен для ускорения работы - чтобы не проверять файлы, которые не были изменены (дата изменения директории говорит о том, когда последний раз были изменены файлы, кот-е в ней находятся)

	if (!st) mkdir((backupdir+dir).c_str());//Создаём директорию, если её не существует

	if (needForUpdateFiles && st/*проверяем также, существует ли вообще директория, т.к. если нет, то незачем проверять её файлы для удаления*/)
	{
		//Удаляем файлы, которых нет в источнике
		if ((hFile=_findfirst((backupdir+dir+"\\*.*").c_str(), &c_file))==-1L)
			printf("Files not found!\n");//Эта строка никогда не должна быть исполнена
		else
		{
			do
			{
				string fullname=dir+"\\"+c_file.name;
	
				if (c_file.attrib & _A_SUBDIR)
				{
					if (strcmp(c_file.name,".") && strcmp(c_file.name,".."))
						if (!fexist(srcdir+fullname,true)) removedir(backupdir+fullname);//Директории нет в источнике => удаляем её и в резервной копии
				}
				else
					if (!fexist(srcdir+fullname)) remove((backupdir+fullname).c_str());//Файла нет в источнике => удаляем его и в резервной копии
			} while(_findnext(hFile, &c_file) == 0);
	
			_findclose(hFile);
		}
	}

	//Обходим текущую директорию источника
	if ((hFile=_findfirst((srcdir+dir+"\\*.*").c_str(), &c_file))==-1L)
		printf("Files not found!\n");//Эта строка никогда не должна быть исполнена
	else
	{
		do
		{
			string fullname=dir+"\\"+c_file.name;

			if (c_file.attrib & _A_SUBDIR)
			{
				if (strcmp(c_file.name,".") && strcmp(c_file.name,".."))
					parse(fullname,c_file.time_write);
			}
			else
			{
				if (/*needForUpdateFiles && */(!(st=fexist(backupdir+fullname))/*файла не существует*/ || c_file.time_write>st->st_mtime/*файл в источнике новее, чем в резервной копии*/))//Обновляем файл
				{
					string tempfname=st ? (backupdir+fullname+".~tmp") : (backupdir+fullname);
					FILE *fsrc=fopen((srcdir+fullname).c_str(),"rb"),*fdest=fopen(tempfname.c_str(),"wb");//Пишем во временный файл (только когда копируемый файл уже существует в резервной копии), чтобы в случае повреждения файла из источника осталась резервная копия
					if (!fdest) {ToLog("Ошибка записи файла. Возможно недостаточно места на диске.\n"); abort();}
					cout<<"Copying: "<<(backupdir+fullname)<<endl;
					bool erroroccured=false;
					if (!fsrc)
						erroroccured=true;
					else
					{
						while (!feof(fsrc))
						{
							size_t size=fread(buf,1,BLOCKSIZE,fsrc);
							if (ferror(fsrc))//Ошибка чтения
							{
								erroroccured=true;
								break;
							}
							fwrite(buf,size,1,fdest);
						}
						fclose(fsrc);
					}
					fclose(fdest);

					if (!erroroccured)//Копирование прошло успешно
					{
						if (st)//Только если файл уже существовал в резервной копии
						{
							remove((backupdir+fullname).c_str());//Удаляем старую версию из резервной копии
							rename(tempfname.c_str(),(backupdir+fullname).c_str());//Меняем имя временного файла на имя скопированного файла (это даёт возможность сохранить версию файла из резервной копии в случае, когда чтение файла из источника не завершилось успешно)
						}
						//Устанавливаем время изменения файла в копии соотвественно источнику
						utimbuf utb={time(NULL),c_file.time_write};
						utime((backupdir+fullname).c_str(),&utb);
					}
					else
					{
						remove(tempfname.c_str());//Удаляем временный/не до конца скопированный файл
						ToLog(("Ошибка чтения файла: "+srcdir+fullname+"\n").c_str());
					}
				}

				//Сохраняем файл, изменённый за последний день
				if (c_file.time_write>=curtime/* && c_file.time_write<curtime+24*3600*/)
				{
					makepath((backupdir+backuppostfix+dir).c_str());

					FILE *fsrc=fopen((srcdir+fullname).c_str(),"rb"),*fdest=fopen((backupdir+backuppostfix+fullname).c_str(),"wb");
					if (!fdest) {ToLog("Ошибка записи файла. Возможно недостаточно места на диске.\n"); abort();}
					if (fsrc)
					{
						while (!feof(fsrc))
						{
							static char buf[BLOCKSIZE];
							size_t size=fread(buf,1,BLOCKSIZE,fsrc);
							if (ferror(fsrc)) break;//Ошибка чтения
							fwrite(buf,size,1,fdest);
						}
						fclose(fsrc);
					}
					fclose(fdest);

					//Устанавливаем время изменения файла в копии соотвественно источнику
					utimbuf utb={time(NULL),c_file.time_write};
					utime((backupdir+backuppostfix+fullname).c_str(),&utb);
				}
			}
		} while(_findnext(hFile, &c_file) == 0);

		_findclose(hFile);
	}
}

/*
const char *dirmakeabsolute(const char *dir)
{
	if (strchr(dir,':')) return dir;
	static char buf[1000];
	getcwd(buf,1000);
	strcat(buf,"\\");
	strcat(buf,dir);
	return buf;
}
*/

FILE *fmlist;

void makemodifylist(const string &dir,int level=0)
{
	struct _finddata_t c_file;
	long hFile;

	if ((hFile=_findfirst((backupdir+backuppostfix+dir+"\\*.*").c_str(), &c_file))!=-1L)
	{
		do
		{
			if (strcmp(c_file.name,".") && strcmp(c_file.name,".."))
			{
				for(int i=0;i<level;i++) fputs("  ",fmlist);
				fprintf(fmlist,"%s\n",c_file.name);
	
				if (c_file.attrib & _A_SUBDIR)
					makemodifylist(dir+"\\"+c_file.name,level+1);
			}
		} while(_findnext(hFile, &c_file) == 0);

		_findclose(hFile);
	}
}

int main(int argc, char* argv[])
{
	if (argc!=3) {cout<<"Program usage:\nbackup <src dir> <backup dir>\n"; return 0;}

	cout<<"Source directory: "<<(srcdir=argv[1])<<"\nBackup directory: "<<(backupdir=argv[2])<<endl<<endl;

	struct stat *st=fexist(srcdir,true);
	if (!st) {cout<<"Source directory not found"; return 0;}

//	if (!fexist(backupdir,true)) mkdir(backupdir.c_str());
	remove("backup.log");

	//Вычисляем текущее время (время начала суток, обновления файлов за которые нужно сохранить)
	curtime=time(NULL)-24*3600;
	struct tm *lt=localtime(&curtime);
	sprintf(backuppostfix,"_%02i.%02i.%4i",lt->tm_mday,lt->tm_mon+1,lt->tm_year+1900);
	lt->tm_sec = lt->tm_min = lt->tm_hour = 0;
	curtime=mktime(lt);

	//Удаляем папку(и) изменений за день более чем недельной давности
	struct _finddata_t c_file;
	long hFile;

	if ((hFile=_findfirst((backupdir+"_*").c_str(), &c_file))!=-1L)
	{
		const char *lastslash=strrchr(backupdir.c_str(),'\\');
		string bupdir,buppath;
		if (lastslash)
		{
			bupdir=lastslash+1;
			buppath=backupdir.substr(0,lastslash-backupdir.c_str());
		}
		else
		{
			bupdir=backupdir;
			buppath=".";
		}

		do
		{
			if ((c_file.attrib & _A_SUBDIR) && strcmp(c_file.name,".") && strcmp(c_file.name,".."))
			{
				int day,mon,year;
				if (sscanf(c_file.name,(bupdir+"_%02i.%02i.%4i").c_str(),&day,&mon,&year)==3)
				{
					struct tm t={0};
					t.tm_isdst=lt->tm_isdst;
					t.tm_mday=day;
					t.tm_mon =mon-1;
					t.tm_year=year-1900;
					if (curtime-mktime(&t)>=15*24*3600) removedir(buppath+"\\"+c_file.name);
				}
			}
		} while(_findnext(hFile, &c_file) == 0);

		_findclose(hFile);
	}

	//Обходим папку источника в поисках изменений относительно резервной копии и обновляем копию, а также сохраняем файлы, изменённые за последний день в отдельную папку
	parse("",st->st_mtime);

	//Создаём файл со списком изменёных файлов
	fmlist=fopen((string(backuppostfix+1)+".txt").c_str(),"wt");
	makemodifylist("");
	fclose(fmlist);
}
