<?php
header('Content-Type: text/html; charset=windows-1251');

if (isset($_POST['variants']))
{
	$f=fopen('votings_data/next','r+b');
	if ($f===FALSE) exit();
	$next=(int)fgets($f);
	rewind($f);
	ftruncate($f,0);
	fwrite($f,(string)($next+1));
	fclose($f);

	$vars=preg_split('/\r?\n/',trim($_POST['variants']));
	$f=fopen("votings_data/$next",'wb');
	if ($f===FALSE) exit();
	fwrite($f,implode(',',array_fill(0,count($vars),'0')).';');
	fclose($f);

	$code=array();
	for ($i=0;$i<count($vars);$i++)
//	    $code[]="[url=http://tav/voting_system/vote.php?id=$next&for=$i][img=http://tav/voting_system/vote_img.php?id=$next&for=$i][/url] - $vars[$i]";
	    $code[]="[url=http://tav.fatal.ru/voting_system/vote.php?id=$next&for=$i][img=http://tav.fatal.ru/voting_system/vote_img.php?id=$next&for=$i][/url] - $vars[$i]";

	$code=implode("\n\n",$code);

	echo <<<EOD
<html>
<body>
Голосование успешно добавлено.<br>
Для использования на форуме просто вставьте в первое сообщение этот код:<br>
<pre style="border:dashed 1px blue">
Для голосования щёлкните по одной из полосок.

$code
</pre>
</body>
</html>
EOD;
}
?>