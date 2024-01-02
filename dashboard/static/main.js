//Device1
function Device1ON(){
  document.getElementById("FAN").innerHTML = "FAN ON";
}
function Device1OFF(){
    document.getElementById("FAN").innerHTML = "FAN OFF ";
}
function ButtonOFF1(){
        document.getElementById("ButtonOFF1").style.background="red";
        document.getElementById("ButtonON1").style.background="white";
}
function ButtonON1(){
        document.getElementById("ButtonON1").style.background="green";
        document.getElementById("ButtonOFF1").style.background="white";
}