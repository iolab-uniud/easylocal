#ifndef OBSERVABLEOBJECT_HH_
#define OBSERVABLEOBJECT_HH_

class ObservableObject
{
public:
    // Observer attaching/detaching
    virtual void AttachObserver(Observer* obs) = 0;
    virtual void DetachObserver(Observer* obs) = 0;
};

#endif /*OBSERVABLEOBJECT_HH_*/
