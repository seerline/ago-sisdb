@echo "start..."

@copy modules.c ..\..\src\.
@set sisdb_path=.\src\win
@if exist %sisdb_path% (
	cd %sisdb_path%
	cmake -G "Visual Studio 12 2013 Win64" ..
) else (
	cd .\src\
	mkdir win
	cd win
	cmake -G "Visual Studio 12 2013 Win64" ..
)
@cd ..\..\
@copy src\api*.h include 
@echo "ok"
@pause


@REM -G "Visual Studio 12 2013 Win64"
@REM clean:
@REM cd ./src/mq/ && rmdir -rf out

