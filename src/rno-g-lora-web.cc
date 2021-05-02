#include "crow_all.h" 






int main(int nargs, char ** args) 
{
  crow::SimpleApp app; 

  CROW_ROUTE(app,"/")( []()
  {
    return "<html><head><title>Hello!</title></head><body><h1>Hello!</h1></body></html>"; 
  }); 

  app.port(1777).multithreaded().run(); 

}
