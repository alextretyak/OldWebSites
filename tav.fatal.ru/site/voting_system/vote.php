<?php
header("Pragma: no-cache");
header('Content-Type: text/html; charset=windows-1251');

if (strpos(@$_SERVER['HTTP_REFERER'],'gamedev.ru')===FALSE)
	exit('��� ����� �� ������, �.�. � HTTP-������� ������ REFERER ���������� ��� �����������.');

header('refresh: 4; url='.$_SERVER['HTTP_REFERER']);

if (isset($_GET['id'],$_GET['for']))
{
	$arr=(@$_COOKIE['TAVsVotings'] ? explode('-',$_COOKIE["TAVsVotings"]) : array());
	$ff=explode(';',file_get_contents("votings_data/$_GET[id]"));
	$vars=explode(',',$ff[0]);
	$ips=($ff[1] ? explode(',',$ff[1]) : array());
	if (array_search($_GET['id'],$arr)!==FALSE ||
		array_search($_SERVER['REMOTE_ADDR'],$ips)!==FALSE)
	{
		echo '�� ��� ����������!';
	}
	else
	{
	    $arr[]=$_GET['id'];
		setcookie('TAVsVotings',implode('-',$arr),0x7fffffff);

		$f=fopen("votings_data/$_GET[id]",'wb');
		if ($f===FALSE) exit();
		$vars[$_GET['for']]++;
		$ips[]=$_SERVER['REMOTE_ADDR'];
		fwrite($f,implode(',',$vars).';'.implode(',',$ips).";$_SERVER[HTTP_REFERER]");
		fclose($f);

		echo '��� ����� ������.';
	}
	echo '<br><br>����� 4 ������� �� ������ �������������� �� ��������, ������ ������.<br><br><a target="_blank" href="howto.html">������ ������������ ����� ����������� � ���� ������?</a>';
}
?>