echo on
echo ��ʼ����win7_x64�汾 wbNdis.sys
echo  ***********************************************��������**************************************************
set tmp_path=F:\wb_net\ndis
set xp_driver_file_src=%tmp_path%\objfre_win7_amd64\amd64\wbNdis.sys
set xp_driver_file_dst=%tmp_path%\output\
set driver_inf_file=%tmp_path%\wbNdis.inf
set msg=win7_x64�汾 wbNdis.sys����
D:
cd %WINDDK%\bin\
call setenv.bat %WINDDK%\ fre x64 WIN7

echo ***********************************************��ʼ��������***********************************************

F:
mkdir %xp_driver_file_dst%

cd %tmp_path%
call build -ceZ
if "%errorlevel%" == "0" (
echo %msg%�ɹ�
echo ��ʼ�ƶ��ļ�
move /y %xp_driver_file_src% "%xp_driver_file_dst%"
copy /y %driver_inf_file% "%xp_driver_file_dst%"
) else echo %msg%��ʧ��

taskkill /f /im oacrmonitor.exe /t

pause
exit


