```
@startuml
skinparam backgroundColor #EEEBDC
skinparam handwritten true

skinparam sequence {
	ArrowColor Black
	ActorBorderColor Blue
	LifeLineBorderColor blue
	LifeLineBackgroundColor #A9DCDF
	
	ParticipantBorderColor DeepSkyBlue
	ParticipantBackgroundColor DeepSkyBlue
	ParticipantFontName Impact
	ParticipantFontSize 17
	ParticipantFontColor #A9DCDF
	
	ActorBackgroundColor DeepSkyBlue
	ActorFontColor Black
	ActorFontSize 17
	ActorFontName Aapex
}

title OTA Flow
actor device
database "OTA Service" as ota

device -> ota: GET /fota.json?dev=device_id&token=access_key&version=version
ota -> device: {"type":"update", "data": {"path":"/path/to/ota-app.bin", "checksum":"checksum", "session": "session_key"}}
device -> ota: GET /path/to/ota-app.bin&dev=id&token=access_key&session=session_key
ota --> device: Response firmware
device <--> device: Restart to bootloader, flash new firmware


@enduml
```
