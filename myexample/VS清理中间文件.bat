@echo off  
setlocal enabledelayedexpansion    
  
for /r . %%a in (testDebug) do (    
  if exist %%a (  
  echo "delete" %%a  
  rd /s /q "%%a"   
 )  
)  
  
for /r . %%a in (testRelease) do (    
  if exist %%a (  
  echo "delete" %%a  
  rd /s /q "%%a"   
 )  
)  
  
for /r . %%a in (ipch) do (    
  if exist %%a (  
  echo "delete" %%a  
  rd /s /q "%%a"   
 )  
)  
  
for /r . %%a in (*.sdf) do (    
  if exist %%a (  
  echo "delete" %%a  
  del "%%a"   
 )  
)  

for /r . %%a in (*.ppm) do (    
  if exist %%a (  
  echo "delete" %%a  
  del "%%a"   
 )  
)
  
pause  