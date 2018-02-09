<?php
header("Pragma: no-cache");
header("Content-type: image/png");

if (isset($_GET['id'],$_GET['for']))
{
	define('WIDTH',400);
	define('HEIGHT',15);
	$im = @imagecreate(WIDTH+2, HEIGHT) or die ("Cannot Initialize new GD image stream");
	imagecolorallocate($im, 155, 20, 32);
	$left_color =imagecolorallocate($im,  34, 136,  45);
	$right_color=imagecolorallocate($im,  49,  46,  86);
	$text_color =imagecolorallocate($im, 240, 240, 240);

	//Считаем общее число голосов
	$totalVotes=0;
	$ff=explode(';',file_get_contents("votings_data/$_GET[id]"));
	$vars=explode(',',$ff[0]);
	for ($i=0;$i<count($vars);$i++)
		$totalVotes+=$vars[$i];

	$votes=(int)$vars[$_GET['for']];

	if ($totalVotes!=0) $n=$votes/$totalVotes; else $n=0;

	if ($_GET['for']==0)//Выводим общее кол-во голосов
		$totalVotes='/'.$totalVotes;
	else $totalVotes='';
	$votes=(int)round($n*100)."% ($votes$totalVotes)";

	$edge=(int)round(WIDTH*$n);
	if ($edge>0)
		imagefilledrectangle($im,1,1,$edge,HEIGHT-2,$left_color);
	if ($edge<WIDTH)
		imagefilledrectangle($im,$edge+1,1,WIDTH,HEIGHT-2,$right_color);
	imagestring($im,1,2,2,$votes,$text_color);

	imagepng($im);
}
?>