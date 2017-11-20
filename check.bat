for %%f in (Scene\*.ss) do @(
	bin\parsess.exe %%f | grep "NOP"
	if not errorlevel 1 echo %%f
)