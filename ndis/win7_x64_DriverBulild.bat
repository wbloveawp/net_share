echo on
echo 开始编译win7_x64版本 wbNdis.sys
echo  ***********************************************变量定义**************************************************
set tmp_path=F:\wb_net\ndis
set xp_driver_file_src=%tmp_path%\objfre_win7_amd64\amd64\wbNdis.sys
set xp_driver_file_dst=%tmp_path%\output\
set driver_inf_file=%tmp_path%\wbNdis.inf
set msg=win7_x64版本 wbNdis.sys编译
D:
cd %WINDDK%\bin\
call setenv.bat %WINDDK%\ fre x64 WIN7

echo ***********************************************开始编译驱动***********************************************

F:
mkdir %xp_driver_file_dst%

cd %tmp_path%
call build -ceZ
if "%errorlevel%" == "0" (
echo %msg%成功
echo 开始移动文件
move /y %xp_driver_file_src% "%xp_driver_file_dst%"
copy /y %driver_inf_file% "%xp_driver_file_dst%"
) else echo %msg%译失败

taskkill /f /im oacrmonitor.exe /t

pause
exit


