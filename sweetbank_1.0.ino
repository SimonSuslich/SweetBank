/*
  File: sweetbank_1.0.ino
  Author: Simon Niklas Suslich
  Date: 2024-02-22
  Description: SweetBank är ett simpelt banksystem, där man kan skapa nya användare, depositera och dra "pengar" från kontot, skicka "pengar" mellan två användare, och kolla fullständig information om kontot.
  Använt hårdvara: Arduino UNO, keypad 3x4, mfrc552 (rfid-läsare), kablar, breadboard rfid-kort och rfid taggar.
  Informationen ges som output i seriella monitorn. 
  Vissa funktioner kräver att en admin skannar sin tagg, vilket är gjort av säkerhetsjäl. 
 */

//Inkludera 2 bibliotek för rfid-läsaren och 1 bibliotek för keypadden.

#include <SPI.h>      //include the SPI bus library
#include <MFRC522.h>  //include the RFID reader library
#include <Keypad.h>

//Definiera varabler för mfrc522 pins

const int SS_PIN = 10;                  //slave select pin
const int RST_PIN = 9;                  //reset pin


//Skapa objekt och strukturer för rfid-läsaren
MFRC522 mfrc522(SS_PIN, RST_PIN);  // instatiate a MFRC522 reader object.
MFRC522::MIFARE_Key key;           //create a MIFARE_Key struct named 'key', which will hold the card information
MFRC522 rfid(SS_PIN, RST_PIN);     // Instance of the class

// skappa byte som man sparar informationen om kortet man skannade och hårdskriven uid för admin taggen
byte nuidPICC[4];
byte adminTac[4] = { 218,1,199,106 };  //Hardwired

// skapa 2D  array med chars i keypad.
const byte ROWS = 4;  //four rows
const byte COLS = 3;  //three columns
char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};
byte rowPins[ROWS] = { 3, 8, 7, 5 };  //connect to the row pinouts of the keypad
byte colPins[COLS] = { 4, 2, 6 };     //connect to the column pinouts of the keypad

//Skapa karta för keys med variablarna, definierade uppe
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


// Class Bank, som lagrar användarnas information. Innehåller kontots ID, pinkod och pengar på kontot. Dessa element är private. Funktionerna är i public: visa konto info, få kontots ID, vilket är samma som UID på kortet, få första elementet i UID (användbart för funktionen isRegistered()), få pinkod, få antal pengar på kontot, sätta pengar på kontot och dra pengar från kontot.
class Bank {
private:
  byte AccUID[4];
  String PinCode;
  int Amount;

public:
  Bank() {
    for (int i = 0; i < 4; i++) {
      AccUID[i] = 0;
    }
    Amount = 0;
    PinCode = "";
  }

  Bank(byte accountUID[4], int amount, String code) {
    for (int i = 0; i < 4; i++) {
      AccUID[i] = accountUID[i];
    }
    Amount = amount;
    PinCode = code;
  }
  //Run metod, visar kontots information
  void showAccInfo() {
    Serial.println("Account information:");
    Serial.print("Account ID - ");
    for (int i = 0;i < 4;i++) {
      Serial.print(AccUID[i]);
      Serial.print(" ");
    }
    Serial.println(" ");
    Serial.println("Amount - " + String(Amount));
    Serial.println("Pin code - " + String(PinCode));
  }
  //Return metod, returnerar byte kontots ID (kortets UID)
  byte *getAccUID() {
    return AccUID;
  }
  // Return metod, returnerar int kontots ID första element
  byte getAccUIDIndexZero() {
    return AccUID[0];
  }
  // Return metod, returnerar String pinkod
  String getPinCode() {
    return PinCode;
  }
  // Return metod, returnerar in pengar på kontot
  int getAmount() {
    return Amount;
  }
  // Run metod, ökar pengar på kontot beroende på argumentet int amount
  void DepositMoney(int amount) {
    Amount += amount;
  }
  // Run metod, minskar pengar på kontot beroende på argumentet int amount, validerar mängden pengar på kontot
  void WithdrawMoney(int amount) {
    if (amount > Amount) {
      Serial.println("You cannot withdraw more money than there is on the account!");
    } else {
      Amount -= amount;
    }
  }
};

// lista på alla användare, tillåter 10 avändare.
Bank allAccounts[10] = {};

//variabel som beskriver om det behövs skriva info meddelandet i Seriella monitorn, som ger instruktioner på hur produkten bör användas. 
bool instructionsPrinted = false;

//Void setup initialiserar Seriell kommunikaiton, rfid-läsaren, och sätter debounce tid för knapparna på keypadden. Skriver ett välkomnsmeddelande och ber om att trycka på en random knapp för att starta programmet.
void setup() {
  Serial.begin(9600);

  SPI.begin();      // Init SPI bus
  rfid.PCD_Init();  // Init MFRC522

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;  //keyByte is defined in the "MIFARE_Key" 'struct' definition in the .h file of the library
  }

  mfrc522.PCD_Init();  // Init MFRC522 card (in case you wonder what PCD means: proximity coupling device)
  Serial.println("Scan a MIFARE Classic card");

  keypad.setDebounceTime(50);  // Set debounce time if needed

  Serial.println("SweetBank");
  Serial.println("Press any key to start...");
  String keyPress = String(keypad.getKey());
  while (keyPress == "") {
    keyPress = String(keypad.getKey());
  }
}



// Void loopen skriver ut info om hur knapparna funkar, läser av eventuell input från keypad
//Om input är: 
//0 - visar fullständigt info om kontot, kräver admin; 
//1 - skapar ett nytt konto, kräver admin; 
//2 - skickar pengar mellan två konton; 
//3 - sätta mer pengar; 
//4 - drar pengar från kontot;
void loop() {

  if (!instructionsPrinted) {
    Serial.println("\n\nUse the keypad to execute a command.\n- Press '0' to check account's information by scanning card (Admin) \n- Press '1' to create a new bank user (Admin) \n- Press '2' to transfer money between two users \n- Press '3' to deposit more money to an account \n- Press '4' to withdraw money from an account\n");
    instructionsPrinted = true;
  }

  //KeyPad as input
  char keyPress = keypad.getKey();

  //Om keyPress == '0', skannar admin taggen (fortsätter vidare om den stämmer, annars ger "Error:..."), skannar kortet, sparar kontots index i allAccounts[] och kallar på metoden showAccInfo(), sätter instruktionsPrinted till false
  if (keyPress == '0') {
    Serial.println("Scan admin tac:");
    getRfidUID();
    if (scanAdminTac(nuidPICC)) {
      clearnuidPICC();

      Serial.println("Scan the card to access account info");
      getRfidUID();
      int userIndex = findAccountIndex(nuidPICC);
      clearnuidPICC();
      allAccounts[userIndex].showAccInfo();
      instructionsPrinted = false;
    } else {
      Serial.println("Error: tac is not admin");
    }
  }

  // Likt keypress == 0, skannar admin taggen, sen kallar på createUSer(); sätter instruktionsPrinted till false
  if (keyPress == '1') {  
    Serial.println("Scan admin tac:");
    getRfidUID();
    if (scanAdminTac(nuidPICC)) {
      clearnuidPICC();
      createUser();
    } else {
      Serial.println("Error: tac is not admin");
    }
    instructionsPrinted = false;
  }

  // Skannar sändarens kort, sparar indexen i arrayen, tar emot transaktionstorleken, sändarens pinkod och skannar mottagarens kort och sparar dess index i allAccaounts[], kallar på transferMoney, med given information som argument, sätter instruktionsPrinted till false
  if (keyPress == '2') {
    Serial.println("Scan Senders Card");
    getRfidUID();
    int indexSender = findAccountIndex(nuidPICC);
    clearnuidPICC();
    Serial.println("Put transfer amount (USD)");
    int amountToTransfer = keyPadWriteInputNumber();
    Serial.println("Write Pin Code");
    String pinCode = keyPadWriteCode();
    Serial.println("Scan Recievers Card");
    getRfidUID();
    int indexReciever = findAccountIndex(nuidPICC);
    clearnuidPICC();
    transferMoney(allAccounts[indexSender], allAccounts[indexReciever], amountToTransfer, pinCode);
    instructionsPrinted = false;
  }
// Skannar kortet, sparar indexen i allAccaounts[], tar emot ett tal som input (hur mycket pengar som ska adderas), kallar på metoden DepositMoney(int), visar hur mycket pengar som finns nu, sätter instruktionsPrinted till false
  if (keyPress == '3') {
    Serial.println("Scan the card to deposit money");
    getRfidUID();
    int userIndex = findAccountIndex(nuidPICC);
    clearnuidPICC();
    Serial.println("Assign amount to deposit (USD)");
    int depositAmount = keyPadWriteInputNumber();
    allAccounts[userIndex].DepositMoney(depositAmount);
    Serial.println("\nAmount - " + String(allAccounts[userIndex].getAmount()));
    instructionsPrinted = false;
  }

// Exakt som den föregående (keypress == 3), men istället kallar på metoden WithdrawMoney(int)
  if (keyPress == '4') {
    Serial.println("Scan the card to withdraw money");
    getRfidUID();
    int userIndex = findAccountIndex(nuidPICC);
    clearnuidPICC();
    Serial.println("Assign amount to deposit (USD)");
    int depositAmount = keyPadWriteInputNumber();
    allAccounts[userIndex].WithdrawMoney(depositAmount);
    Serial.println("\nAmount - " + String(allAccounts[userIndex].getAmount()));
    instructionsPrinted = false;
  }
}

// Run funktion som läser av ett rfid-kort och sparar UID i nuidPICC

  //loopar till den hittar ett kort
  //validerar om nuid blev läst
  //sparar uid i nuidPICC[]
  //avslutar kryptering och picc

void getRfidUID() {
  // Look for new cards
  while (!rfid.PICC_IsNewCardPresent()) {
    //Loop while searching for card
  }
  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial()) {
    return;  // Return nullptr to indicate failure
  }
  // Store NUID into nuidPICC array
  for (byte i = 0; i < 4; i++) {
    nuidPICC[i] = rfid.uid.uidByte[i];
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// Run funktion som tömmer nuidPICC för att undvika att fel konto används.
// Loopar igenom alla element i nuidPICC och tillderar värdet 0
void clearnuidPICC() {
  for (byte i = 0; i < 4; i++) {
    nuidPICC[i] = 0;
  }
}

// Run funktion som skapar nytt konto, skannar kortet (validerar om kontot redan är registrerat), tar emot hur mycket pengar som ska vara på kontot, pinkoden. Loopar igenom allAccounts och hittar ett tomt element. Sparar kontots information. Displayar informationen en sista gång.

  //Skapar variablar för konto ID, balans och pinkod
  //sätter newAccID's alla element till 0
  //Läser av kort (kallar på getRfidUID())
  //kopierar nuidPICC till newAccID
  //validerar om kortet redan har ett konto
  //Tar ett heltal som input (keyPadWriteInputNumber()), tilldelar värdet till newAmount
  //tar 4 siffror som input och omvandlar till string (keyPad), tilldelar värdet till newPinCode
  //loopar igenom alla konton som finns, hittar det första tomma stället och skapar ett nytt konto med den givna informationen
  //bryter ut ur loopen

void createUser() {
  byte newAccID[4];
  int newAmount;
  String newPinCode;

  // Generate random values for new account
  for (int i = 0; i < 4; i++) {
    newAccID[i] = 0;
  }

  Serial.println("Scan the card you want to assign the account to");
  getRfidUID();


  for (int i = 0; i < 4; i++) {
    newAccID[i] = nuidPICC[i];
  }
  clearnuidPICC();

  if (isAccountRegistered(newAccID)) {
    Serial.println("Account already registered");
    return;
  }

  Serial.println("Assign Account Balance (USD)");
  newAmount = keyPadWriteInputNumber();  // Set initial amount to 0
  if (newAmount == "") {
    Serial.println("Error: Command interrupted");
    return;
  }

  Serial.println("Assign Pincode");
  newPinCode = keyPadWriteCode();
  if (newPinCode == "") {
    Serial.println("Error: Command interrupted");
    return;
  }

  // Create a new instance of Bank class and add it to allAccounts array
  for (int i = 0; i < 10; i++) {
    if (allAccounts[i].getAccUIDIndexZero() == 0) {  // Find the first empty slot in the array
      allAccounts[i] = Bank(newAccID, newAmount, newPinCode);
      Serial.println("New account created!");
      allAccounts[i].showAccInfo();
      break;
    }
  }
}

// Run funktion som skickar pengar mellan två konton. Argument: sändarens och mottagarens konton, transaktionsvärde, sändarens pinkod. Validerar pinkoden, validerar om sändaren har tillräckligt med pengar. Genomför transactionen, displayar info om hur mycket som skickades, hur mycket sändaren har kvar och hur mycket mottagaren har. 

  //Validerar om sändarens pinkod stämmer
  //Validerar om sändaren har tillräckligt med pengar
  //kallar på metoder i class Bank för att dra ut pengar och sätta pengar
  //Printar i Seriella monitorn om hur mycket pengar som har skcikats och hur mycket pengar sändaren och mottagaren har

void transferMoney(Bank &sender, Bank &reciever, int transferAmount, String pinCode) {
  if (pinCode != sender.getPinCode()) {
    Serial.println("Error: Incorrect pincode.");
    return;
  }
  int sendersAmount = sender.getAmount();
  if (sendersAmount < transferAmount) {
    Serial.println("Error: Cannot transfer more money than there is on sender's account");
    return;
  }
  sender.WithdrawMoney(transferAmount);
  reciever.DepositMoney(transferAmount);
  Serial.println("Transaction Completed");
  Serial.print("Transaction amount: ");
  Serial.println(transferAmount);
  Serial.print("Senders Bank amount: ");
  Serial.println(sender.getAmount());
  Serial.print("Recievers Bank amount: ");
  Serial.println(reciever.getAmount());
}

// Return funktion (hjälpfunktion) för att validera om konton är redan registrerat banken. Loopar igenom kontons ID och det skannade kortets UID. Argument: nuidPICC; Returnernar en bool.
bool isAccountRegistered(byte AccountUID[4]) {
  for (int i = 0; i < 10; i++) {
    if (allAccounts[i].getAccUIDIndexZero() == AccountUID[0]) {
      bool isRegistered = true;
      for (int j = 1; j < 4; j++) {
        if (*(allAccounts[i].getAccUID() + j) != AccountUID[j]) {
          isRegistered = false;
          break;
        }
      }
      if (isRegistered) {
        return true;
      }
    }
  }
  return false;
}

//Return funktion (hjälpfunktion) som loopar igenom alla kontons första index i UID, om den hittar match med argumentet fortsätter skanna och returnerar indexen som användaren har i listan allAccounts[]. Argument: nuidPICC, returnerar en int. 
int findAccountIndex;byte AccountUID[4]) {
  if (!isAccountRegistered(AccountUID)) {
    return;
  }
  for (int i = 0;i < 10;i++) {
    if (allAccounts[i].getAccUIDIndexZero() == AccountUID[0]) {
      bool isRegistered = true;
      for (int j = 1; j < 4; j++) {
        if (*(allAccounts[i].getAccUID() + j) != AccountUID[j]) {
          isRegistered = false;
          break;
        }
      }
      if (isRegistered) {
        return i;
      }
    }
  }
}

// Funktion som ger ett tal som skrivs på keypadden. Returnerar int tal.

  //definierar variablar för hela "talet" och inputen (kallar på keypadden)
  //loopar till man inte konfirmerar med en "#"
  //Om input är "*", raderar den sista siffran från "amount"
  //Om "amount" är tom och input är "*", avbryter funktionen.
  //Annars läggs till input till Amount
  //returnar amount, transformerar datatyp från String till Int

int keyPadWriteInputNumber() {
  String inputNumber = String(keypad.getKey());
  String amount = "";

  while (inputNumber != "#") {
    if (inputNumber == "*") {
      if (amount == "") {
        Serial.println("Interrupted");
        amount = "";
        return "";
      } else {
        amount = amount.substring(0, amount.length() - 1);
        Serial.println("");
        Serial.println("");
        Serial.println("");
        Serial.print(amount);
        if (amount.length() == 0) {
          Serial.println("Continue by typing a number \nOr intrerrupt by pressing '*'\n");
        }
      }
    } else if (inputNumber != "") {
      amount += inputNumber;
      Serial.print(inputNumber);
    }
    inputNumber = keypad.getKey();
  }
  Serial.println(" ");
  return amount.toInt();
}

// Funktion som används för att skriva pinkoden (string). Returnerar 4 siffror som string (pinkod)

  //Likt funktionen keyPadWriteInputNumber()
  //Enda skillnaden att när inputCode är 4, så breakar loopen och returneras inputCode

String keyPadWriteCode() {
  String inputNumber = String(keypad.getKey());
  String inputCode = "";

  while (inputNumber != "#") {
    if (inputNumber == "*") {
      if (inputCode == "") {
        Serial.println("Interrupted");
        inputCode = "";
        break;
      } else {
        inputCode = inputCode.substring(0, inputCode.length() - 1);
        Serial.println("");
        Serial.println("");
        Serial.println("");
        Serial.print(inputCode);
        if (inputCode.length() == 0) {
          Serial.println("Continue by typing a number \nOr intrerrupt by pressing '*'\n");
        }
      }
    } else if (inputNumber != "") {
      inputCode += inputNumber;
      Serial.print(inputNumber);
    }
    if (inputCode.length() >= 4) {
      break;
    }
    inputNumber = keypad.getKey();
  }
  Serial.println(" ");
  return inputCode;
}

// Return Funktion som validerar om den skannade  taggen tillhör adminen. Argument: nuiPICC, return bool.

  //loopar igenom från talet 0 till 3, 
  //om adminTac[i] inte stämmer med tacUID[i], returnerar false
  //Annars loopar den hela vägen och returnar true

bool scanAdminTac(byte tacUID[4]) {
  for (int i = 0;i < 4;i++) {
    if (adminTac[i] != tacUID[i]) {
      return false;
    }
  }
  return true;
}
