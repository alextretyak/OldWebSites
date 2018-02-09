<?php
header("Content-type: image/png");

$formula=preg_replace('/%([0-9a-f]{2})/ie','chr(0x$1)',@$_SERVER['QUERY_STRING']);
//putenv('GDFONTPATH='.realpath('.'));
define('FONT','./westminster.ttf');
define('FONT_SIZE',12);

$arr=imagettfbbox(FONT_SIZE,0,FONT,$formula);
//print_r($arr);
$im = @imagecreate(abs($arr[2]-$arr[0]), abs($arr[5]-$arr[3])) or die ("Cannot Initialize new GD image stream");
imagecolorallocate($im, 255, 255, 255);
$black=imagecolorallocate($im, 0, 0, 0);

//imagestring($im,4,2,2,$formula,$black);
imagettftext($im,FONT_SIZE,0,0,$arr[1]-$arr[7],$black,FONT,$formula);
imagepng($im);
?>